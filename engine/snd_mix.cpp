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

// snd_mix.cpp -- portable code to mix sounds for snd_dma.cpp

#include "quakedef.h"
#include "sound.h"
#include "pr_cmds.h"
//#include "voice_sound_engine_interface.h"

#include "client.h"
#include "cl_main.h"

#include <ctype.h>

//#include "winquake.h"
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

// hard clip input value to -32767 <= y <= 32767
#define CLIP(x) ((x) > 32767 ? 32767 : ((x) < -32767 ? -32767 : (x)))

portable_samplepair_t paintbuffer[(PAINTBUFFER_SIZE + 1) * 2];

// Used by other C files.
extern "C"
{
	extern void Snd_WriteLinearBlastStereo16( void );
	extern void SND_PaintChannelFrom8( channel_t* ch, sfxcache_t* sc, int count );
	extern int			snd_scaletable[32][256];
	extern int* snd_p, snd_linear_count, snd_vol;
	extern short* snd_out;
}

int			snd_scaletable[32][256];
int* snd_p, snd_linear_count, snd_vol;
short* snd_out;

// TODO: Are we sure to use MAX_CHANNELS macro name? it's also 128
wavstream_t wavstreams[MAX_CHANNELS];

char* rgpparseword[CVOXWORDMAX];

char voxperiod[] = "_period";			// vocal pause
char voxcomma[] = "_comma";				// vocal pause

char szsentences[20] = "sound/sentences.txt"; // sentence file

int cszrawsentences;
char* rgpszrawsentence[CVOXFILESENTENCEMAX];

voxword_t rgrgvoxword[CBSENTENCENAME_MAX][32];

extern bool s_careerAudioPaused;

/*
==================
SetCareerAudioState

Sets Audio State for Career
==================
*/
void SetCareerAudioState( int paused )
{
	s_careerAudioPaused = paused;
}

/*
==================
CareerAudio_Command_f
==================
*/
void CareerAudio_Command_f( void )
{
	SetCareerAudioState(false);
}

/*
===============================================================================

CHANNEL MIXING

===============================================================================
*/

//===============================================================================
// Low level mixing routines
//===============================================================================

#if	!id386
void Snd_WriteLinearBlastStereo16( void )
{
	int		i;
	int		val;

	for (i = 0; i < snd_linear_count; i += 2)
	{
		val = (snd_p[i] * snd_vol) >> 8;
		if (val > 0x7fff)
			snd_out[i] = 0x7fff;
		else if (val < (short)0x8000)
			snd_out[i] = (short)0x8000;
		else
			snd_out[i] = val;

		val = (snd_p[i + 1] * snd_vol) >> 8;
		if (val > 0x7fff)
			snd_out[i + 1] = 0x7fff;
		else if (val < (short)0x8000)
			snd_out[i + 1] = (short)0x8000;
		else
			snd_out[i + 1] = val;
	}
}
#endif

// Transfer paintbuffer into dma buffer

void S_TransferStereo16( int end )
{
	int		lpos;
	int		lpaintedtime;
	int		endtime;
	LPVOID  pbuf;
#ifdef _WIN32
	int		reps;
	DWORD	dwSize, dwSize2;
	LPVOID  pbuf2;
	HRESULT	hresult;
#endif

	snd_vol = volume.value * 256;

	snd_p = (int*)paintbuffer;

	lpaintedtime = 2 * paintedtime;
	endtime = 2 * end;

#ifdef _WIN32
	if (pDSBuf)
	{
		reps = 0;

		while (DS_OK != (hresult = pDSBuf->lpVtbl->Lock(pDSBuf, 0, gSndBufSize, &pbuf, &dwSize, &pbuf2, &dwSize2, 0)))
		{
			if (hresult != DSERR_BUFFERLOST)
			{
				Con_Printf("S_TransferStereo16: DS::Lock Sound Buffer Failed\n");
				S_Shutdown();
				S_Startup();
				return;
			}

			if (++reps > 10000)
			{
				Con_Printf("S_TransferStereo16: DS: couldn't restore buffer\n");
				S_Shutdown();
				S_Startup();
				return;
			}
		}
	}
	else
#endif
	{
		pbuf = (DWORD*)shm->buffer;
	}

	while (lpaintedtime < endtime)
	{
		// lpaintedtime - where to start painting into dma buffer. 
		// handle recirculating buffer issues
		// lpos - samplepair index into dma buffer. First samplepair from paintbuffer to be xfered here.
		lpos = lpaintedtime & ((shm->samples >> 1) - 1);

		// snd_out is L/R sample index into dma buffer.  First L sample from paintbuffer goes here.
		snd_out = (short*)pbuf + (lpos << 1);
		
		snd_linear_count = ((shm->samples >> 1) - lpos);

		// clamp snd_linear_count to be only as many samplepairs premixed
		if (lpaintedtime + snd_linear_count > endtime)
			// endtime - lpaintedtime = number of premixed sample pairs ready for xfer.
			snd_linear_count = endtime - lpaintedtime;

		// snd_linear_count is now number of mono 16 bit samples (L and R) to xfer.
		snd_linear_count <<= 1;

		// transfer 16bit samples from snd_p into snd_out, multiplying each sample by volume.
		Snd_WriteLinearBlastStereo16();

		// advance paintbuffer pointer
		snd_p += snd_linear_count;

		// advance lpaintedtime by number of samplepairs just xfered.
		lpaintedtime += (snd_linear_count >> 1);
	}

#ifdef _WIN32
	if (pDSBuf)
		pDSBuf->lpVtbl->Unlock(pDSBuf, pbuf, dwSize, NULL, 0);
#endif
}

// Transfer contents of main paintbuffer PAINTBUFFER out to 
// device.  Perform volume multiply on each sample.

void S_TransferPaintBuffer( int endtime )
{
	S_TransferStereo16(endtime);
}

// free channel so that it may be allocated by the
// next request to play a sound.  If sound is a 
// word in a sentence, release the sentence.
// Works for static, dynamic, sentence and stream sounds

void S_FreeChannel( channel_t* ch )
{
	if (ch->entchannel == CHAN_STREAM)
	{
		Wavstream_Close((uint32)((char*)ch - (char*)channels) >> 6);
		Cache_Free(&ch->sfx->cache);
	}
	else if (ch->entchannel >= CHAN_NETWORKVOICE_BASE && ch->entchannel <= CHAN_NETWORKVOICE_END)
	{
		//VoiceSE_NotifyFreeChannel(ch->entchannel); TODO: Implement
	}

	if (ch->isentence >= 0)
		rgrgvoxword[ch->isentence][0].sfx = nullptr;

	ch->isentence = -1;
	ch->sfx = nullptr;

	SND_CloseMouth(ch);
}

bool S_CheckWavEnd( channel_t* ch, sfxcache_t** psc, int ltime, int ichan )
{
	sfx_t* sfx = nullptr;
	sfxcache_t* sc = nullptr;

	if ((*psc)->length == 0x40000000)
		return false;

	if ((*psc)->loopstart < 0)
	{
		if (ch->entchannel == CHAN_STREAM)
		{
			if (wavstreams[ichan].csamplesplayed < wavstreams[ichan].info.samples)
			{
				Wavstream_GetNextChunk(ch, ch->sfx);
				ch->pos = 0;
				ch->end = (*psc)->length + ltime;
				return false;
			}
		}
		else
		{
			if (ch->isentence >= 0)
			{
				sfx = rgrgvoxword[ch->isentence][ch->iword].sfx;

				if (sfx && !rgrgvoxword[ch->isentence][ch->iword].fKeepCached)
					Cache_Free(&sfx->cache);

				ch->sfx = rgrgvoxword[ch->isentence][ch->iword + 1].sfx;

				if (ch->sfx)
				{
					sc = S_LoadSound(ch->sfx, ch);
					*psc = sc;

					if (sc)
					{
						ch->pos = 0;
						ch->end = ltime + sc->length;
						ch->iword++;
						VOX_TrimStartEndTimes(ch, sc);
						return false;
					}
				}
			}
		}

		S_FreeChannel(ch);
		return true;
	}

	ch->pos = (*psc)->loopstart;
	ch->end = (*psc)->length + ltime - ch->pos;

	if (ch->isentence >= 0)
		rgrgvoxword[ch->isentence][ch->iword].samplefrac = 0;

	return false;
}

// Mix all channels into active paintbuffers until paintbuffer is full or 'end' is reached.
// end: time in 22khz samples to mix
// fPaintHiresSounds: set to true if we are operating with High Resolution sounds
// voiceOnly: true if we are doing voice processings, this sets in S_PaintChannels
//		if the game isn't paused
void S_MixChannelsToPaintbuffer( int end, int fPaintHiresSounds, int voiceOnly )
{
	int i, voxdata = 0;

	char copyBuf[4096];

	int pitch, chend, endt;
	int ltime, timecompress;

	int sampleCount, offs;
	int oldleft, oldright;

	int hires;
	bool bVoice = false;

	short* sfx = nullptr;

	channel_t* ch = nullptr;
	sfxcache_t* sc = nullptr;
	wavstream_t* wavstream = nullptr;

	hires = fPaintHiresSounds != 0;

	for (i = 0; i < total_channels; i++)
	{
		// mix each channel into paintbuffer
		ch = &channels[i];

		wavstream = &wavstreams[i];

		if (!ch->sfx)
			continue;

		// UNDONE: Can get away with not filling the cache until
		// we decide it should be mixed

		sc = S_LoadSound(ch->sfx, ch);

		bVoice = sc->length == 0x40000000;

		if (bVoice && !voiceOnly)
			continue;

		if (!bVoice && voiceOnly)
			continue;

		// Don't mix sound data for sounds with zero volume. If it's a non-looping sound, 
		// just remove the sound when its volume goes to zero.

		if (ch->leftvol || ch->rightvol)
		{
			if ((fPaintHiresSounds && sc->speed > shm->speed) || (!fPaintHiresSounds && sc->speed == shm->speed))
			{
				voxdata = 0;
				timecompress = 0;
				ltime = paintedtime;

				// get playback pitch
				pitch = ch->pitch;

				if (ch->isentence >= 0)
				{
					pitch = ch->pitch + rgrgvoxword[ch->isentence][ch->iword].pitch - 100;

					if (rgrgvoxword[ch->isentence][ch->iword].pitch <= 0)
						pitch = ch->pitch;
					
					timecompress = rgrgvoxword[ch->isentence][ch->iword].timecompress;
				}

				// do this until we reach the end 
				while (ltime < end)
				{
					chend = ch->end;

					// See if painting highres sounds
					if (hires)
						chend = (sc->length >> 1) + ch->end - sc->length;

					if (chend < end)
						endt = chend - ltime;
					else
						endt = end - ltime;

					if (bVoice)
					{
						if (sc->width != 2 || sc->stereo)
							break;
					}

					sampleCount = endt << hires;
					offs = (ltime - paintedtime) << hires;

					if (sampleCount <= 0)
					{
						if (ltime >= chend)
						{
							voxdata = 0;

							if (S_CheckWavEnd(ch, &sc, ltime, i))
								break;
						}

						continue;
					}

					if (sc->width == 1 && ch->entnum > 0)
					{
						if (ch->entchannel == CHAN_VOICE || ch->entchannel == CHAN_STREAM)
							SND_MoveMouth(ch, sc, sampleCount);
					}

					oldleft = ch->leftvol;
					oldright = ch->rightvol;

					if (!bVoice)
					{
						ch->leftvol = oldleft / g_SND_VoiceOverdrive;
						ch->rightvol = oldright / g_SND_VoiceOverdrive;
					}

					// Paint channels
					if (sc->width == 1)
					{
						if ((pitch == PITCH_NORM && !timecompress) || ch->isentence < 0)
						{
							if (ltime == paintedtime)
								SND_PaintChannelFrom8(ch, sc, sampleCount);
							else
								SND_PaintChannelFrom8Offs(paintbuffer, ch, sc, sampleCount, offs);
						}
						else
						{
							voxdata = VOX_FPaintPitchChannelFrom8Offs(paintbuffer, ch, sc, sampleCount, pitch, timecompress, offs);
						}
					}
					else
					{
						if (bVoice)
						{
							sfx = (short*)copyBuf;
							//sampleCount = VoiceSE_GetSoundDataCallback(sc, copyBuf, sizeof(copyBuf), ch->pos, sampleCount); TODO: Implement
						}
						else
						{
							sfx = (short*)&sc->data[ch->pos * sizeof(short)];
						}

						SND_PaintChannelFrom16Offs(paintbuffer, ch, sfx, sampleCount, offs);
					}

					ch->leftvol = oldleft;
					ch->rightvol = oldright;

					ltime += sampleCount >> hires;

					if (ch->entchannel == CHAN_STREAM)
						wavstream->csamplesplayed += sampleCount;

					if (!voxdata)
					{
						if (ltime >= chend)
						{
							voxdata = 0;

							if (S_CheckWavEnd(ch, &sc, ltime, i))
								break;
						}

						continue;
					}

					voxdata = 0;

					if (S_CheckWavEnd(ch, &sc, ltime, i))
						break;
				}
			}
		}
		else if (sc->loopstart < 0)
		{
			S_FreeChannel(ch);
		}
	}
}

void S_PaintChannels( int endtime )
{
	static portable_samplepair_t paintprev;

	int i, end, count;

	while (paintedtime < endtime)
	{
		// mix a full 'paintbuffer' of sound

		// clamp at paintbuffer size

		end = endtime;
		if (end - paintedtime > PAINTBUFFER_SIZE)
			end = paintedtime + PAINTBUFFER_SIZE;

		// number of 44khz samples to mix into paintbuffer, up to paintbuffer size

		count = end - paintedtime;

		// clear all mix buffers

		Q_memset(paintbuffer, 0, count * sizeof(portable_samplepair_t));

		if (!cl.paused && !s_careerAudioPaused)
			S_MixChannelsToPaintbuffer(end, 0, 0);

		if (hisound.value == 0.0)
			SX_RoomFX(count, true, true);

		for (i = count - 1; i; i--)
		{
			paintbuffer[(i * 2) + 1].left = paintbuffer[i].left;
			paintbuffer[(i * 2) + 1].right = paintbuffer[i].right;

			paintbuffer[i * 2].left = (paintbuffer[i - 1].left + paintbuffer[i].left) >> 1;
			paintbuffer[i * 2].right = (paintbuffer[i - 1].right + paintbuffer[i].right) >> 1;
		}

		paintbuffer[1].left = paintbuffer[0].left;
		paintbuffer[1].right = paintbuffer[0].right;

		// use interpolation value from previous mix

		paintbuffer[0].left = (paintbuffer[1].left + paintprev.left) >> 1;
		paintbuffer[0].right = (paintbuffer[1].right + paintprev.right) >> 1;

		paintprev.left = paintbuffer[(count * 2) - 1].left;
		paintprev.right = paintbuffer[(count * 2) - 1].right;

		if (hisound.value > 0.0)
		{
			if (!cl.paused && !s_careerAudioPaused)
				S_MixChannelsToPaintbuffer(end, 1, 0); // mix hires

			SX_RoomFX((count * 2), true, true);
		}

		if (!cl.paused && !s_careerAudioPaused)
			S_MixChannelsToPaintbuffer(end, 1, 1); // mix voice as well

		// transfer out according to DMA format
		S_TransferPaintBuffer(end);
		paintedtime = end;
	}
}

void SND_InitScaletable( void )
{
	int i, j;

	for (i = 0; i < 32; i++)
	{
		for (j = 0; j < 256; j++)
			snd_scaletable[i][j] = ((signed char)j) * i * 8;
	}
}

#if	!id386
void SND_PaintChannelFrom8( channel_t* ch, sfxcache_t* sc, int count )
{
	int 	data;
	int* lscale, * rscale;
	byte* sfx;
	int		i;

	if (ch->leftvol > 255)
		ch->leftvol = 255;
	if (ch->rightvol > 255)
		ch->rightvol = 255;

	lscale = snd_scaletable[ch->leftvol >> 3];
	rscale = snd_scaletable[ch->rightvol >> 3];
	sfx = &sc->data[ch->pos];

	for (i = 0; i < count; i++)
	{
		data = sfx[i];
		paintbuffer[i].left += lscale[data];
		paintbuffer[i].right += rscale[data];
	}

	ch->pos += count;
}

#endif	// !id386

void SND_PaintChannelFrom8Offs( portable_samplepair_t* paintbuffer, channel_t* ch, sfxcache_t* sc, int count, int offset )
{
	int i;
	int	data;
	int* lscale, * rscale;

	byte* sfx = nullptr;
	portable_samplepair_t* pb = &paintbuffer[offset];

	if (ch->leftvol > 255)
		ch->leftvol = 255;
	if (ch->rightvol > 255)
		ch->rightvol = 255;

	lscale = snd_scaletable[ch->leftvol >> 3];
	rscale = snd_scaletable[ch->rightvol >> 3];
	sfx = &sc->data[ch->pos];

	for (i = 0; i < count; i++)
	{
		data = sfx[i];
		pb[i].left += lscale[data];
		pb[i].right += rscale[data];
	}

	ch->pos += count;
}

void SND_PaintChannelFrom16Offs( portable_samplepair_t* paintbuffer, channel_t* ch, short* sfx, int count, int offset )
{
	int i;
	portable_samplepair_t* pb = &paintbuffer[offset];

	if (count > 0)
	{
		for (i = 0; i != count; i++)
		{
			pb[i].left += (ch->leftvol * sfx[i]) >> 8;
			pb[i].right += (ch->rightvol * sfx[i]) >> 8;
		}
	}

	ch->pos += count;
}



// E3: gated one-shot, gate by db of original signal in channel_t, or by absolute volume
// E3: 'panner' - pan fx left<->right, front<->back, circle, at mod frequency. ie: large echo fx



// NEXT: check outputs of modulators - normalize to +/- 0-1.0 and all inputs normalized to same
// NEXT: performance tune
// NEXT: rename base routines
// NEXT: rva may use mda if lfo params set
// NEXT: perf 
//		- chained processor cost high? test with nulls and with release build
//		- all getnext funcitons take samplecount and pbuffer!
//		- use globals to cut down on params to function calls
//		- note: release build is  more than 2x faster due to inline calls
//		- note: mmx for buffer mixing, multiplying etc
// NEXT: stereoize delays - alternate taps with l/r as if sound is reflecting
// NEXT: filter only rva output

// NEXT: test ptc, crs, flt, env, efo
// NEXT: add MDY to RVA
// NEXT: add clipper/distorter, amplifier, noise generator LFO

// NEXT: stereoize all room sounds using l/r roomsize delays
// NEXT: stereo delay using l/r/u/d/f/b dimensions of room and wall materials
// NEXT: spatialize all sounds based on inter-ear time delay (headphone mode)
// NEXT: filter all sounds based on inter-ear filter (headphone mode)



//===============================================================================
//
// Digital Signal Processing algorithms for audio FX.
//
// KellyB 1/24/97
//===============================================================================


#define SXDLY_MAX		0.400							// max delay in seconds
#define SXRVB_MAX		0.100							// max reverb reflection time
#define SXSTE_MAX		0.100							// max stereo delay line time

using sample_t = short;									// delay lines must be 32 bit, now that we have 16 bit samples

struct dlyline_t
{
	int cdelaysamplesmax;								// size of delay line in samples
	int lp;												// lowpass flag 0 = off, 1 = on

	int idelayinput;									// i/o indices into circular delay line
	int idelayoutput;

	int idelayoutputxf;									// crossfade output pointer
	int xfade;											// crossfade value

	int delaysamples;									// current delay setting
	int delayfeed;										// current feedback setting

	int lp0, lp1, lp2;									// lowpass filter buffer

	int mod;											// sample modulation count
	int modcur;

	HANDLE hdelayline;									// handle to delay line buffer
	sample_t* lpdelayline;								// buffer
};

#define CSXDLYMAX		4

#define ISXMONODLY		0								// mono delay line
#define ISXRVB			1								// first of the reverb delay lines
#define CSXRVBMAX		2
#define ISXSTEREODLY	3								// 50ms left side delay

dlyline_t rgsxdly[CSXDLYMAX];							// array of delay lines

#define gdly0 (rgsxdly[ISXMONODLY])
#define gdly1 (rgsxdly[ISXRVB])
#define gdly2 (rgsxdly[ISXRVB + 1])
#define gdly3 (rgsxdly[ISXSTEREODLY])

#define CSXLPMAX		10								// lowpass filter memory

int rgsxlp[CSXLPMAX];

int sxamodl, sxamodr;									// amplitude modulation values
int sxamodlt, sxamodrt;									// modulation targets
int sxmod1, sxmod2;
int sxmod1cur, sxmod2cur;

// Mono Delay parameters

cvar_t sxdly_delay = { "room_delay", "0" };				// current delay in seconds
cvar_t sxdly_feedback = { "room_feedback", "0.2" };		// cyles
cvar_t sxdly_lp = { "room_dlylp", "1.0" };				// lowpass filter

float sxdly_delayprev;									// previous delay setting value

// Mono Reverb parameters

cvar_t sxrvb_size = { "room_size", "0" };				// room size 0 (off) 0.1 small - 0.35 huge
cvar_t sxrvb_feedback = { "room_refl", "0.7" };			// reverb decay 0.1 short - 0.9 long
cvar_t sxrvb_lp = { "room_rvblp", "1.0" };				// lowpass filter

float sxrvb_sizeprev;

// Stereo delay (no feedback)

cvar_t sxste_delay = { "room_left", "0" };				// straight left delay
float sxste_delayprev;

// Underwater/special fx modulations

cvar_t sxmod_lowpass = { "room_lp", "0" };
cvar_t sxmod_mod = { "room_mod", "0" };

// Main interface

cvar_t sxroom_type = { "room_type", "0" };
cvar_t sxroomwater_type = { "waterroom_type", "14" };
float sxroom_typeprev;

cvar_t sxroom_off = { "room_off", "0" };

int sxhires = 0;
int sxhiresprev = 0;

bool SXDLY_Init( int idelay, float delay );
void SXDLY_Free( int idelay );
void SXDLY_DoDelay( int count );
void SXRVB_DoReverb( int count );
void SXDLY_DoStereoDelay( int count );
void SXRVB_DoAMod( int count );

//=====================================================================
// Init/release all structures for sound effects
//=====================================================================

void SX_Init( void )
{
	Q_memset(rgsxdly, 0, sizeof(dlyline_t) * CSXDLYMAX);
	Q_memset(rgsxlp, 0, sizeof(int) * CSXLPMAX);

	sxdly_delayprev = -1.0;
	sxrvb_sizeprev = -1.0;
	sxste_delayprev = -1.0;
	sxroom_typeprev = -1.0;

	// flag hires sound mode
	sxhires = hisound.value != 0.0;
	sxhiresprev = sxhires;

	// init amplitude modulation params

	sxamodl = sxamodr = 255;
	sxamodlt = sxamodrt = 255;

	if (shm)
	{
		sxmod1 = 350 * (shm->speed / SOUND_11k);	// 11k was the original sample rate all dsp was tuned at
		sxmod2 = 450 * (shm->speed / SOUND_11k);
	}

	sxmod1cur = sxmod1;
	sxmod2cur = sxmod2;

	Cvar_RegisterVariable(&sxdly_delay);
	Cvar_RegisterVariable(&sxdly_feedback);
	Cvar_RegisterVariable(&sxdly_lp);
	Cvar_RegisterVariable(&sxrvb_size);
	Cvar_RegisterVariable(&sxrvb_feedback);
	Cvar_RegisterVariable(&sxrvb_lp);
	Cvar_RegisterVariable(&sxste_delay);
	Cvar_RegisterVariable(&sxmod_lowpass);
	Cvar_RegisterVariable(&sxmod_mod);
	Cvar_RegisterVariable(&sxroom_type);
	Cvar_RegisterVariable(&sxroomwater_type);
	Cvar_RegisterVariable(&sxroom_off);
}

void SX_Free( void )
{
	int i;

	// release mono delay line

	SXDLY_Free(ISXMONODLY);

	// release reverb lines

	for (i = 0; i < CSXRVBMAX; i++)
		SXDLY_Free(i + ISXRVB);

	SXDLY_Free(ISXSTEREODLY);
}

// Set up a delay line buffer allowing a max delay of 'delay' seconds 
// Frees current buffer if it already exists. idelay indicates which of 
// the available delay lines to init.

bool SXDLY_Init( int idelay, float delay )
{
	int cbsamples;
	dlyline_t* pdly = &rgsxdly[idelay];

	HPSTR lpData = nullptr;
	HANDLE hData = nullptr;

	if (delay > SXDLY_MAX)
		delay = SXDLY_MAX;

	if (pdly->lpdelayline)
	{
		GlobalUnlock(pdly->hdelayline);
		GlobalFree(pdly->hdelayline);

		pdly->hdelayline = nullptr;
		pdly->lpdelayline = nullptr;
	}

	if (delay == 0.0)
		return true;

	pdly->cdelaysamplesmax = (int)((float)shm->speed * delay) << sxhires;
	pdly->cdelaysamplesmax += 1;

	cbsamples = pdly->cdelaysamplesmax * sizeof(sample_t);

	hData = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE, cbsamples);

	if (!hData)
	{
		Con_SafePrintf("Sound FX: Out of memory.\n");
		return false;
	}

	lpData = (HPSTR)GlobalLock(hData);

	if (!lpData)
	{
		Con_SafePrintf("Sound FX: Failed to lock.\n");
		GlobalFree(hData);
		return false;
	}

	Q_memset(lpData, 0, cbsamples);

	pdly->hdelayline = hData;

	pdly->lpdelayline = (sample_t*)lpData;

	// init delay loop input and output counters.

	// NOTE: init of idelayoutput only valid if pdly->delaysamples is set
	// NOTE: before this call!

	pdly->idelayinput = 0;
	pdly->idelayoutput = pdly->cdelaysamplesmax - pdly->delaysamples;
	pdly->xfade = 0;
	pdly->lp = 1;
	pdly->mod = 0;
	pdly->modcur = 0;

	// init lowpass filter memory

	pdly->lp0 = pdly->lp1 = pdly->lp2 = 0;

	return true;
}

// release delay buffer and deactivate delay

void SXDLY_Free( int idelay )
{
	dlyline_t* pdly = &rgsxdly[idelay];

	if (pdly->lpdelayline)
	{
		GlobalUnlock(pdly->hdelayline);
		GlobalFree(pdly->hdelayline);

		pdly->hdelayline = nullptr;
		pdly->lpdelayline = nullptr;
	}
}

// check for new stereo delay param

void SXDLY_CheckNewStereoDelayVal( void )
{
	int delaysamples;
	dlyline_t* pdly = &rgsxdly[ISXSTEREODLY];

	// set up stereo delay

	if (sxste_delay.value != sxste_delayprev)
	{
		if (sxste_delay.value == 0.0)
		{
			// deactivate delay line

			SXDLY_Free(ISXSTEREODLY);
			sxste_delayprev = 0.0;
		}
		else
		{
			delaysamples = (int)(min(sxste_delay.value, SXSTE_MAX) * (float)shm->speed) << sxhires;

			// init delay line if not active

			if (pdly->lpdelayline)
			{
				pdly->delaysamples = delaysamples;

				SXDLY_Init(ISXSTEREODLY, SXSTE_MAX);
			}

			// do crossfade to new delay if delay has changed

			if (delaysamples != pdly->delaysamples)
			{
				// set up crossfade from old pdly->delaysamples to new delaysamples

				pdly->idelayoutputxf = pdly->idelayinput - delaysamples;

				if (pdly->idelayoutputxf < 0)
					pdly->idelayoutputxf += pdly->cdelaysamplesmax;

				pdly->xfade = 128;
			}

			sxste_delayprev = sxste_delay.value;

			// UNDONE: modulation disabled
			//pdly->mod = 500 * (shm->speed / SOUND_11k);		// change delay every n samples
			pdly->mod = 0;
			pdly->modcur = pdly->mod;

			// deactivate line if rounded down to 0 delay

			if (pdly->delaysamples == 0)
				SXDLY_Free(ISXSTEREODLY);
		}
	}
}

// stereo delay, left channel only, no feedback

void SXDLY_DoStereoDelay( int count )
{
	int left, countr;
	sample_t sampledly, samplexf;

	portable_samplepair_t* pbuf = nullptr;

	// process delay line if active

	if (rgsxdly[ISXSTEREODLY].lpdelayline)
	{
		pbuf = paintbuffer;
		countr = count;

		// process each sample in the paintbuffer...

		while (countr--)
		{
			if (gdly3.mod && (--gdly3.modcur < 0))
				gdly3.modcur = gdly3.mod;

			// get delay line sample from left line

			sampledly = *(gdly3.lpdelayline + gdly3.idelayoutput);
			left = pbuf->left;

			// only process if left value or delayline value are non-zero or xfading

			if (gdly3.xfade || sampledly || left)
			{
				// if we're not crossfading, and we're not modulating, but we'd like to be modulating,
				// then setup a new crossfade.

				if (!gdly3.xfade && !gdly3.modcur && gdly3.mod)
				{
					// set up crossfade to new delay value, if we're not already doing an xfade

					//gdly3.idelayoutputxf = gdly3.idelayoutput + 
					//		((RandomLong(0,0x7FFF) * gdly3.delaysamples) / (RAND_MAX * 2)); // 100 = ~ 9ms

					gdly3.idelayoutputxf = gdly3.idelayoutput +
						((RandomLong(0, 0xFF) * gdly3.delaysamples) >> 9); // 100 = ~ 9ms

					if (gdly3.idelayoutputxf >= gdly3.cdelaysamplesmax)
						gdly3.idelayoutputxf -= gdly3.cdelaysamplesmax;

					gdly3.xfade = 128;
				}

				// modify sampledly if crossfading to new delay value

				if (gdly3.xfade)
				{
					samplexf = (*(gdly3.lpdelayline + gdly3.idelayoutputxf) * (128 - gdly3.xfade)) >> 7;
					sampledly = ((sampledly * gdly3.xfade) >> 7) + samplexf;

					if (++gdly3.idelayoutputxf >= gdly3.cdelaysamplesmax)
						gdly3.idelayoutputxf = 0;

					if (--gdly3.xfade == 0)
						gdly3.idelayoutput = gdly3.idelayoutputxf;
				}

				// save output value into delay line

				left = CLIP(left);

				*(gdly3.lpdelayline + gdly3.idelayinput) = left;

				// save delay line sample into output buffer
				pbuf->left = sampledly;
			}
			else
			{
				// keep clearing out delay line, even if no signal in or out

				*(gdly3.lpdelayline + gdly3.idelayinput) = 0;
			}

			// update delay buffer pointers

			if (++gdly3.idelayinput >= gdly3.cdelaysamplesmax)
				gdly3.idelayinput = 0;

			if (++gdly3.idelayoutput >= gdly3.cdelaysamplesmax)
				gdly3.idelayoutput = 0;

			pbuf++;
		}
	}
}

// If sxdly_delay or sxdly_feedback have changed, update delaysamples
// and delayfeed values.  This applies only to delay 0, the main echo line.

void SXDLY_CheckNewDelayVal( void )
{
	dlyline_t* pdly = &rgsxdly[ISXMONODLY];

	if (sxdly_delay.value != sxdly_delayprev)
	{
		if (sxdly_delay.value == 0.0)
		{
			// deactivate delay line

			SXDLY_Free(ISXMONODLY);
			sxdly_delayprev = sxdly_delay.value;
		}
		else
		{
			// init delay line if not active

			pdly->delaysamples = (int)(min(sxdly_delay.value, SXDLY_MAX) * (float)shm->speed) << sxhires;
			
			if (pdly->lpdelayline == nullptr)
				SXDLY_Init(ISXMONODLY, SXDLY_MAX);

			// flush delay line and filters

			if (pdly->lpdelayline)
			{
				Q_memset(pdly->lpdelayline, 0, pdly->cdelaysamplesmax * sizeof(sample_t));
				pdly->lp0 = 0;
				pdly->lp1 = 0;
				pdly->lp2 = 0;
			}

			// init delay loop input and output counters

			pdly->idelayinput = 0;
			pdly->idelayoutput = pdly->cdelaysamplesmax - pdly->delaysamples;

			sxdly_delayprev = sxdly_delay.value;

			// deactivate line if rounded down to 0 delay

			if (pdly->delaysamples == 0)
				SXDLY_Free(ISXMONODLY);
		}
	}

	pdly->lp = (int)sxdly_lp.value;
	pdly->delayfeed = sxdly_feedback.value * 255;
}

// This routine updates both left and right output with 
// the mono delayed signal.  Delay is set through console vars room_delay
// and room_feedback.

void SXDLY_DoDelay( int count )
{
	int val;
	int valt;
	int left;
	int right;

	int countr;
	int sampledly;

	dlyline_t* pdly = nullptr;
	portable_samplepair_t* pbuf = nullptr;

	pdly = &rgsxdly[ISXMONODLY];

	// process mono delay line if active

	if (pdly->lpdelayline)
	{
		pbuf = paintbuffer;
		countr = count;

		// process each sample in the paintbuffer...

		while (countr--)
		{
			// get delay line sample

			sampledly = pdly->lpdelayline[pdly->idelayoutput];

			left = pbuf->left;
			right = pbuf->right;

			// only process if delay line and paintbuffer samples are non zero

			if (sampledly || left || right)
			{
				// get current sample from delay buffer

				// calculate delayed value from avg of left and right channels

				val = ((left + right) >> 1) + ((pdly->delayfeed * sampledly) >> 8);

				// limit val to short
				val = CLIP(val);

				// lowpass

				if (pdly->lp)
				{
					valt = (pdly->lp0 + pdly->lp1 + 2 * val) >> 2;

					pdly->lp0 = pdly->lp1;
					pdly->lp1 = val;
				}
				else
				{
					valt = val;
				}

				// store delay output value into output buffer

				pdly->lpdelayline[pdly->idelayinput] = valt;

				pbuf->left = left + (valt >> 2);
				pbuf->right = right + (valt >> 2);

				// Now clip those to short
				pbuf->left = CLIP(pbuf->left);
				pbuf->right = CLIP(pbuf->right);
			}
			else
			{
				// not playing samples, but must still flush lowpass buffer and delay line
				valt = pdly->lp0 = pdly->lp1 = 0;

				pdly->lpdelayline[pdly->idelayinput] = valt;
			}

			// update delay buffer pointers

			if (++gdly0.idelayinput >= gdly0.cdelaysamplesmax)
				gdly0.idelayinput = 0;

			if (++gdly0.idelayoutput >= gdly0.cdelaysamplesmax)
				gdly0.idelayoutput = 0;

			pbuf++;
		}
	}
}

// Check for a parameter change on the reverb processor

#define RVB_XFADE	 32	// xfade time between new delays
#define RVB_MODRATE1 (500 * (shm->speed / SOUND_11k))	// how often, in samples, to change delay (1st rvb)
#define RVB_MODRATE2 (700 * (shm->speed / SOUND_11k))	// how often, in samples, to change delay (2nd rvb)

void SXRVB_CheckNewReverbVal( void )
{
	int i;
	int	mod;
	int delaysamples;

	dlyline_t* pdly = nullptr;

	if (sxrvb_size.value != sxrvb_sizeprev)
	{
		sxrvb_sizeprev = sxrvb_size.value;

		if (sxrvb_size.value == 0.0)
		{
			// deactivate all delay lines

			SXDLY_Free(ISXRVB);
			SXDLY_Free(ISXRVB + 1);
		}
		else
		{
			for (i = ISXRVB; i < ISXRVB + CSXRVBMAX; i++)
			{
				// init delay line if not active

				pdly = &rgsxdly[i];

				switch (i)
				{
					case ISXRVB:
						delaysamples = (int)(min(sxrvb_size.value, SXRVB_MAX) * (float)shm->speed) << sxhires;
						pdly->mod = RVB_MODRATE1 << sxhires;
						break;

					case ISXRVB + 1:
						delaysamples = (int)(min(sxrvb_size.value * 0.71, SXRVB_MAX) * (float)shm->speed) << sxhires;
						pdly->mod = RVB_MODRATE2 << sxhires;
						break;

					default:
						delaysamples = 0;
						break;
				}

				mod = pdly->mod;				// KB: bug, SXDLY_Init clears mod, modcur, xfade and lp - save mod before call

				if (pdly->lpdelayline == nullptr)
				{
					pdly->delaysamples = delaysamples;

					SXDLY_Init(i, SXRVB_MAX);
				}

				pdly->modcur = pdly->mod = mod;	// KB: bug, SXDLY_Init clears mod, modcur, xfade and lp - restore mod after call

				// do crossfade to new delay if delay has changed

				if (delaysamples != pdly->delaysamples)
				{
					// set up crossfade from old pdly->delaysamples to new delaysamples

					pdly->idelayoutputxf = pdly->idelayinput - delaysamples;

					if (pdly->idelayoutputxf < 0)
						pdly->idelayoutputxf += pdly->cdelaysamplesmax;

					pdly->xfade = RVB_XFADE;
				}

				// deactivate line if rounded down to 0 delay

				if (pdly->delaysamples == 0)
					SXDLY_Free(i);
			}
		}
	}

	rgsxdly[ISXRVB].delayfeed = sxrvb_feedback.value * 255;
	rgsxdly[ISXRVB].lp = sxrvb_lp.value;

	rgsxdly[ISXRVB + 1].delayfeed = sxrvb_feedback.value * 255;
	rgsxdly[ISXRVB + 1].lp = sxrvb_lp.value;
}

// main routine for updating the paintbuffer with new reverb values.
// This routine updates both left and right lines with 
// the mono reverb signal.  Delay is set through console vars room_reverb
// and room_feedback.  2 reverbs operating in parallel.

void SXRVB_DoReverb( int count )
{
	int val;
	int valt;
	int left;
	int right;
	sample_t sampledly;
	sample_t samplexf;
	portable_samplepair_t* pbuf;
	int countr;
	int voutm;
	int vlr;

	// process reverb lines if active

	if (rgsxdly[ISXRVB].lpdelayline)
	{
		pbuf = paintbuffer;
		countr = count;

		// process each sample in the paintbuffer...

		while (countr--)
		{
			left = pbuf->left;
			right = pbuf->right;
			voutm = 0;
			vlr = (left + right) >> 1;

			// UNDONE: ignored
			if (--gdly1.modcur < 0)
				gdly1.modcur = gdly1.mod;

			// ========================== ISXRVB============================	

			// get sample from delay line

			sampledly = *(gdly1.lpdelayline + gdly1.idelayoutput);

			// only process if something is non-zero

			if (gdly1.xfade || sampledly || left || right)
			{
				// modulate delay rate
				// UNDONE: modulation disabled
				if (0 && !gdly1.xfade && !gdly1.modcur && gdly1.mod)
				{
					// set up crossfade to new delay value, if we're not already doing an xfade

					//gdly1.idelayoutputxf = gdly1.idelayoutput + 
					//		((RandomLong(0,0x7FFF) * gdly1.delaysamples) / (RAND_MAX * 2)); // performance

					gdly1.idelayoutputxf = gdly1.idelayoutput +
						((RandomLong(0, 0xFF) * gdly1.delaysamples) >> 9); // 100 = ~ 9ms

					if (gdly1.idelayoutputxf >= gdly1.cdelaysamplesmax)
						gdly1.idelayoutputxf -= gdly1.cdelaysamplesmax;

					gdly1.xfade = RVB_XFADE;
				}

				// modify sampledly if crossfading to new delay value

				if (gdly1.xfade)
				{
					samplexf = (*(gdly1.lpdelayline + gdly1.idelayoutputxf) * (RVB_XFADE - gdly1.xfade)) / RVB_XFADE;
					sampledly = ((sampledly * gdly1.xfade) / RVB_XFADE) + samplexf;

					if (++gdly1.idelayoutputxf >= gdly1.cdelaysamplesmax)
						gdly1.idelayoutputxf = 0;

					if (--gdly1.xfade == 0)
						gdly1.idelayoutput = gdly1.idelayoutputxf;
				}

				if (sampledly)
				{
					// get current sample from delay buffer

					// calculate delayed value from avg of left and right channels

					val = vlr + ((gdly1.delayfeed * sampledly) >> 8);

					// limit to short
					val = CLIP(val);
				}
				else
				{
					val = vlr;
				}

				// lowpass

				if (gdly1.lp)
				{
					valt = (gdly1.lp0 + val) >> 1;
					gdly1.lp0 = val;
				}
				else
				{
					valt = val;
				}

				// store delay output value into output buffer

				*(gdly1.lpdelayline + gdly1.idelayinput) = valt;

				voutm = valt;
			}
			else
			{
				// not playing samples, but still must flush lowpass buffer & delay line

				gdly1.lp0 = gdly1.lp1 = 0;
				*(gdly1.lpdelayline + gdly1.idelayinput) = 0;

				voutm = 0;
			}

			// update delay buffer pointers

			if (++gdly1.idelayinput >= gdly1.cdelaysamplesmax)
				gdly1.idelayinput = 0;

			if (++gdly1.idelayoutput >= gdly1.cdelaysamplesmax)
				gdly1.idelayoutput = 0;

			// ========================== ISXRVB + 1========================

			// UNDONE: ignored
			if (--gdly2.modcur < 0)
				gdly2.modcur = gdly2.mod;

			if (gdly2.lpdelayline)
			{
				// get sample from delay line

				sampledly = *(gdly2.lpdelayline + gdly2.idelayoutput);

				// only process if something is non-zero

				if (gdly2.xfade || sampledly || left || right)
				{
					// UNDONE: modulation disabled
					if (0 && !gdly2.xfade && gdly2.modcur && gdly2.mod)
					{
						// set up crossfade to new delay value, if we're not already doing an xfade

						//gdly2.idelayoutputxf = gdly2.idelayoutput + 
						//		((RandomLong(0,RAND_MAX) * gdly2.delaysamples) / (RAND_MAX * 2)); // performance

						gdly2.idelayoutputxf = gdly2.idelayoutput +
							((RandomLong(0, 0xFF) * gdly2.delaysamples) >> 9); // 100 = ~ 9ms


						if (gdly2.idelayoutputxf >= gdly2.cdelaysamplesmax)
							gdly2.idelayoutputxf -= gdly2.cdelaysamplesmax;

						gdly2.xfade = RVB_XFADE;
					}

					// modify sampledly if crossfading to new delay value

					if (gdly2.xfade)
					{
						samplexf = (*(gdly2.lpdelayline + gdly2.idelayoutputxf) * (RVB_XFADE - gdly2.xfade)) / RVB_XFADE;
						sampledly = ((sampledly * gdly2.xfade) / RVB_XFADE) + samplexf;

						if (++gdly2.idelayoutputxf >= gdly2.cdelaysamplesmax)
							gdly2.idelayoutputxf = 0;

						if (--gdly2.xfade == 0)
							gdly2.idelayoutput = gdly2.idelayoutputxf;
					}

					if (sampledly)
					{
						// get current sample from delay buffer

						// calculate delayed value from avg of left and right channels

						val = vlr + ((gdly2.delayfeed * sampledly) >> 8);

						// limit to short
						val = CLIP(val);	
					}
					else
					{
						val = vlr;
					}

					// lowpass

					if (gdly2.lp)
					{
						valt = (gdly2.lp0 + val) >> 1;
						gdly2.lp0 = val;
					}
					else
					{
						valt = val;
					}

					// store delay output value into output buffer

					*(gdly2.lpdelayline + gdly2.idelayinput) = valt;

					voutm += valt;
				}
				else
				{
					// not playing samples, but still must flush lowpass buffer

					gdly2.lp0 = gdly2.lp1 = 0;
					*(gdly2.lpdelayline + gdly2.idelayinput) = 0;
				}

				// update delay buffer pointers

				if (++gdly2.idelayinput >= gdly2.cdelaysamplesmax)
					gdly2.idelayinput = 0;

				if (++gdly2.idelayoutput >= gdly2.cdelaysamplesmax)
					gdly2.idelayoutput = 0;
			}

			// ============================ Mix================================

			// add mono delay to left and right channels

			// drop output by inverse of cascaded gain for both reverbs
			voutm = (11 * voutm) >> 6;

			left = CLIP((voutm + left));
			right = CLIP((voutm + right));

			pbuf->left = left;
			pbuf->right = right;

			pbuf++;
		}
	}
}

// amplitude modulator, low pass filter for underwater weirdness

void SXRVB_DoAMod( int count )
{
	int valtl, valtr;
	int left;
	int right;

	int fLowpass;
	int fmod;

	int countr;

	portable_samplepair_t* pbuf = nullptr;

	// process reverb lines if active

	if (sxmod_lowpass.value != 0.0 || sxmod_mod.value != 0.0)
	{
		pbuf = paintbuffer;
		countr = count;

		fLowpass = sxmod_lowpass.value != 0.0;
		fmod = sxmod_mod.value != 0.0;

		// process each sample in the paintbuffer...

		while (countr--)
		{
			left = pbuf->left;
			right = pbuf->right;

			// only process if non-zero

			if (fLowpass)
			{
				valtl = left;
				valtr = right;

				left = (rgsxlp[0] + rgsxlp[1] + rgsxlp[2] + rgsxlp[3] + rgsxlp[4] + left);
				right = (rgsxlp[5] + rgsxlp[6] + rgsxlp[7] + rgsxlp[8] + rgsxlp[9] + right);

				left >>= 2;
				right >>= 2;

				rgsxlp[4] = valtl;
				rgsxlp[9] = valtr;

				rgsxlp[0] = rgsxlp[1];
				rgsxlp[1] = rgsxlp[2];
				rgsxlp[2] = rgsxlp[3];
				rgsxlp[3] = rgsxlp[4];
				rgsxlp[4] = rgsxlp[5];
				rgsxlp[5] = rgsxlp[6];
				rgsxlp[6] = rgsxlp[7];
				rgsxlp[7] = rgsxlp[8];
				rgsxlp[8] = rgsxlp[9];
			}

			if (fmod)
			{
				if (--sxmod1cur < 0)
					sxmod1cur = sxmod1;

				if (!sxmod1)
					sxamodlt = RandomLong(32, 255);

				if (--sxmod2cur < 0)
					sxmod2cur = sxmod2;

				if (!sxmod2)
					sxamodlt = RandomLong(32, 255);

				left = (left * sxamodl) >> 8;
				right = (right * sxamodr) >> 8;

				if (sxamodl < sxamodlt)
					sxamodl++;
				else if (sxamodl > sxamodlt)
					sxamodl--;

				if (sxamodr < sxamodrt)
					sxamodr++;
				else if (sxamodr > sxamodrt)
					sxamodr--;
			}

			left = CLIP(left);
			right = CLIP(right);

			pbuf->left = left;
			pbuf->right = right;

			pbuf++;
		}
	}
}

struct sx_preset_t
{
	float room_lp;					// for water fx, lowpass for entire room
	float room_mod;					// stereo amplitude modulation for room

	float room_size;				// reverb: initial reflection size
	float room_refl;				// reverb: decay time
	float room_rvblp;				// reverb: low pass filtering level

	float room_delay;				// mono delay: delay time
	float room_feedback;			// mono delay: decay time
	float room_dlylp;				// mono delay: low pass filtering level

	float room_left;				// left channel delay time
};


sx_preset_t rgsxpre[CSXROOM] =
{

// SXROOM_OFF					0

//	lp		mod		size	refl	rvblp	delay	feedbk	dlylp	left  
	{0.0,	0.0,	0.0,	0.0,	1.0,	0.0,	0.0,	2.0,	0.0},

// SXROOM_GENERIC				1		// general, low reflective, diffuse room

//	lp		mod		size	refl	rvblp	delay	feedbk	dlylp	left  
	{0.0,	0.0,	0.0,	0.0,	1.0,	0.065,	0.1,	0.0,	0.01},

// SXROOM_METALIC_S				2		// highly reflective, parallel surfaces
// SXROOM_METALIC_M				3
// SXROOM_METALIC_L				4

//	lp		mod		size	refl	rvblp	delay	feedbk	dlylp	left  
	{0.0,	0.0,	0.0,	0.0,	1.0,	0.02,	0.75,	0.0,	0.01}, // 0.001
	{0.0,	0.0,	0.0,	0.0,	1.0,	0.03,	0.78,	0.0,	0.02}, // 0.002
	{0.0,	0.0,	0.0,	0.0,	1.0,	0.06,	0.77,	0.0,	0.03}, // 0.003


// SXROOM_TUNNEL_S				5		// resonant reflective, long surfaces
// SXROOM_TUNNEL_M				6
// SXROOM_TUNNEL_L				7

//	lp		mod		size	refl	rvblp	delay	feedbk	dlylp	left  
	{0.0,	0.0,	0.05,	0.85,	1.0,	0.008,	0.96,	2.0,	0.01}, // 0.01
	{0.0,	0.0,	0.05,	0.88,	1.0,	0.010,	0.98,	2.0,	0.02}, // 0.02
	{0.0,	0.0,	0.05,	0.92,	1.0,	0.015,	0.995,	2.0,	0.04}, // 0.04

// SXROOM_CHAMBER_S				8		// diffuse, moderately reflective surfaces
// SXROOM_CHAMBER_M				9
// SXROOM_CHAMBER_L				10

//	lp		mod		size	refl	rvblp	delay	feedbk	dlylp	left  
	{0.0,	0.0,	0.05,	0.84,	1.0,	0.0,	0.0,	2.0,	0.012}, // 0.003
	{0.0,	0.0,	0.05,	0.90,	1.0,	0.0,	0.0,	2.0,	0.008}, // 0.002
	{0.0,	0.0,	0.05,	0.95,	1.0,	0.0,	0.0,	2.0,	0.004}, // 0.001

// SXROOM_BRITE_S				11		// diffuse, highly reflective
// SXROOM_BRITE_M				12
// SXROOM_BRITE_L				13

//	lp		mod		size	refl	rvblp	delay	feedbk	dlylp	left  
	{0.0,	0.0,	0.05,	0.7,	0.0,	0.0,	0.0,	2.0,	0.012}, // 0.003
	{0.0,	0.0,	0.055,	0.78,	0.0,	0.0,	0.0,	2.0,	0.008}, // 0.002
	{0.0,	0.0,	0.05,	0.86,	0.0,	0.0,	0.0,	2.0,	0.002}, // 0.001

// SXROOM_WATER1				14		// underwater fx
// SXROOM_WATER2				15
// SXROOM_WATER3				16

//	lp		mod		size	refl	rvblp	delay	feedbk	dlylp	left  
	{1.0,	0.0,	0.0,	0.0,	1.0,	0.0,	0.0,	2.0,	0.01},
	{1.0,	0.0,	0.0,	0.0,	1.0,	0.06,	0.85,	2.0,	0.02},
	{1.0,	0.0,	0.0,	0.0,	1.0,	0.2,	0.6,	2.0,	0.05},

// SXROOM_CONCRETE_S			17		// bare, reflective, parallel surfaces
// SXROOM_CONCRETE_M			18
// SXROOM_CONCRETE_L			19

//	lp		mod		size	refl	rvblp	delay	feedbk	dlylp	left  
	{0.0,	0.0,	0.05,	0.8,	1.0,	0.0,	0.48,	2.0,	0.016}, // 0.15 delay, 0.008 left
	{0.0,	0.0,	0.06,	0.9,	1.0,	0.0,	0.52,	2.0,	0.01 }, // 0.22 delay, 0.005 left
	{0.0,	0.0,	0.07,	0.94,	1.0,	0.3,	0.6,	2.0,	0.008}, // 0.001

// SXROOM_OUTSIDE1				20		// echoing, moderately reflective
// SXROOM_OUTSIDE2				21		// echoing, dull
// SXROOM_OUTSIDE3				22		// echoing, very dull

//	lp		mod		size	refl	rvblp	delay	feedbk	dlylp	left  
	{0.0,	0.0,	0.0,	0.0,	1.0,	0.3,	0.42,	2.0,	0.0},
	{0.0,	0.0,	0.0,	0.0,	1.0,	0.35,	0.48,	2.0,	0.0},
	{0.0,	0.0,	0.0,	0.0,	1.0,	0.38,	0.6,	2.0,	0.0},

// SXROOM_CAVERN_S				23		// large, echoing area
// SXROOM_CAVERN_M				24
// SXROOM_CAVERN_L				25

//	lp		mod		size	refl	rvblp	delay	feedbk	dlylp	left  
	{0.0,	0.0,	0.05,	0.9,	1.0,	0.2,	0.28,	0.0,	0.0},
	{0.0,	0.0,	0.07,	0.9,	1.0,	0.3,	0.4,	0.0,	0.0},
	{0.0,	0.0,	0.09,	0.9,	1.0,	0.35,	0.5,	0.0,	0.0},

// SXROOM_WEIRDO1				26	
// SXROOM_WEIRDO2				27
// SXROOM_WEIRDO3				28
// SXROOM_WEIRDO3				29

//	lp		mod		size	refl	rvblp	delay	feedbk	dlylp	left  
	{0.0,	1.0,	0.01,	0.9,	0.0,	0.0,	0.0,	2.0,	0.05},
	{0.0,	0.0,	0.0,	0.0,	1.0,	0.009,	0.999,	2.0,	0.04},
	{0.0,	0.0,	0.001,	0.999,	0.0,	0.2,	0.8,	2.0,	0.05}

};



// force next call to sx_roomfx to reload all room parameters.
// used when switching to/from hires sound mode.

void SX_ReloadRoomFX( void )
{
	// reset all roomtype parms

	sxroom_typeprev = -1.0;

	sxdly_delayprev = -1.0;
	sxrvb_sizeprev = -1.0;
	sxste_delayprev = -1.0;

	// UNDONE: handle sxmod and mod parms? 
}

// main routine for processing room sound fx
// if fFilter is TRUE, then run in-line filter (for underwater fx)
// if fTimefx is TRUE, then run reverb and delay fx
// NOTE: only processes preset room_types from 0-29 (CSXROOM)

void SX_RoomFX( int count, int fFilter, int fTimefx )
{
	int i;
	float roomType;

	bool bReset = false;

	// return right away if fx processing is turned off

	if (sxroom_off.value != 0.0)
		return;

	// detect changes in hires sound param

	sxhires = (hisound.value == 0 ? 0 : 1);

	if (sxhires != sxhiresprev)
	{
		SX_ReloadRoomFX();
		sxhiresprev = sxhires;
	}

	if (cl.waterlevel > 2)
		roomType = sxroomwater_type.value;
	else
		roomType = sxroom_type.value;

	if (roomType != sxroom_typeprev)
	{
		//Con_Printf("Room_type: %2.1f\n", roomType);

		sxroom_typeprev = roomType;

		i = (int)(roomType);

		if (i < CSXROOM && i >= 0)
		{
			// Set hardcoded values from rgsxpre table
			Cvar_SetValue("room_lp", rgsxpre[i].room_lp);
			Cvar_SetValue("room_mod", rgsxpre[i].room_mod);
			Cvar_SetValue("room_size", rgsxpre[i].room_size);
			Cvar_SetValue("room_refl", rgsxpre[i].room_refl);
			Cvar_SetValue("room_rvblp", rgsxpre[i].room_rvblp);
			Cvar_SetValue("room_delay", rgsxpre[i].room_delay);
			Cvar_SetValue("room_feedback", rgsxpre[i].room_feedback);
			Cvar_SetValue("room_dlylp", rgsxpre[i].room_dlylp);
			Cvar_SetValue("room_left", rgsxpre[i].room_left);
		}

		SXRVB_CheckNewReverbVal();
		SXDLY_CheckNewDelayVal();
		SXDLY_CheckNewStereoDelayVal();

		bReset = true;
	}

	if (bReset || roomType != 0.0)
	{
		// debug code
		SXRVB_CheckNewReverbVal();
		SXDLY_CheckNewDelayVal();
		SXDLY_CheckNewStereoDelayVal();
		// debug code

		if (fFilter)
			SXRVB_DoAMod(count);

		if (fTimefx)
		{
			SXRVB_DoReverb(count);
			SXDLY_DoDelay(count);
			SXDLY_DoStereoDelay(count);
		}
	}
}

int Wavstream_Init( void )
{
	int i;

	for (i = 0; i < MAX_CHANNELS; i++)
	{
		Q_memset(&wavstreams[i], 0, sizeof(wavstream_t));
		wavstreams[i].hFile = 0;
	}

	return 1;
}

void Wavstream_Close( int i )
{
	if (wavstreams[i].hFile)
		FS_Close(wavstreams[i].hFile);

	Q_memset(&wavstreams[i], 0, sizeof(wavstream_t));
	wavstreams[i].hFile = 0;
}

void Wavstream_GetNextChunk( channel_t* ch, sfx_t* s )
{
	uint32 idx;
	int cbread;
	int csamples;

	byte* data = nullptr;
	sfxcache_t* sc = nullptr;

	idx = (uint32)((char*)ch - (char*)channels) >> 6;
	sc = (sfxcache_t*)Cache_Check(&s->cache);

	wavstreams[idx].lastposloaded = wavstreams[idx].info.dataofs + wavstreams[idx].csamplesplayed;

	data = COM_LoadFileLimit(nullptr, wavstreams[idx].lastposloaded, 0x8000, &cbread, &wavstreams[idx].hFile);

	csamples = wavstreams[idx].info.samples - wavstreams[idx].csamplesplayed;

	wavstreams[idx].csamplesinmem = cbread;

	if (wavstreams[idx].csamplesinmem > csamples)
		wavstreams[idx].csamplesinmem = csamples;

	sc->length = wavstreams[idx].csamplesinmem;
	ResampleSfx(s, sc->speed, sc->width, data, cbread);
}

//===============================================================================
// Client entity mouth movement code.  Set entity mouthopen variable, based
// on the sound envelope of the voice channel playing.
// KellyB 10/22/97
//===============================================================================

void SND_ForceInitMouth( int entnum )
{
	if (entnum < 0 || entnum >= cl.max_edicts)
		return; // Bad entity number

	cl_entity_t* ent = &cl_entities[entnum];

	ent->mouth.mouthopen = 0;
	ent->mouth.sndavg = 0;
	ent->mouth.sndcount = 0;
}

void SND_ForceCloseMouth( int entnum )
{
	if (entnum >= 0 && entnum < cl.max_edicts)
		cl_entities[entnum].mouth.mouthopen = 0;
}

// called when voice channel is first opened on this entity

void SND_InitMouth( int entnum, int entchannel )
{
	cl_entity_t* ent = nullptr;

	if ((entchannel == CHAN_VOICE || entchannel == CHAN_STREAM) && entnum > 0 && entnum < cl.max_edicts)
	{
		ent = &cl_entities[entnum];
		ent->mouth.mouthopen = 0;
		ent->mouth.sndavg = 0;
		ent->mouth.sndcount = 0;
	}
}

// called when channel stops

void SND_CloseMouth( channel_t* ch )
{
	if (ch->entnum > 0)
	{
		if (ch->entchannel == CHAN_VOICE || ch->entchannel == CHAN_STREAM)
		{
			// shut mouth
			SND_ForceCloseMouth(ch->entnum);
		}
	}
}

#define CAVGSAMPLES 10

void SND_MoveMouth( channel_t* ch, sfxcache_t* sc, int count )
{
	int data;
	int i = 0;
	int savg = 0;
	int scount;
	cl_entity_t* pent = nullptr;

	pent = &cl_entities[ch->entnum];
	scount = pent->mouth.sndcount;

	while (i < count && scount < CAVGSAMPLES)
	{
		data = (char)sc->data[ch->pos + i];
		savg += abs(data);
		
		i += 80 + ((byte)data & 0x1F);
		scount++;
	}

	pent->mouth.sndavg += savg;
	pent->mouth.sndcount = (byte)scount;

	if (pent->mouth.sndcount >= CAVGSAMPLES)
	{
		pent->mouth.mouthopen = pent->mouth.sndavg / CAVGSAMPLES;
		pent->mouth.sndavg = 0;
		pent->mouth.sndcount = 0;
	}
}

void SND_MoveMouth16( int entnum, short* pdata, int count )
{
	int data;
	int i = 0;
	int savg = 0;
	int scount;
	cl_entity_t* pent = nullptr;

	if (entnum < 0 || entnum >= cl.max_edicts)
		return; // Bad entity number

	scount = cl_entities[entnum].mouth.sndcount;
	pent = &cl_entities[entnum];

	while (i < count && scount < CAVGSAMPLES)
	{
		data = pdata[i];
		savg += abs(data);

		i += 80 + ((byte)data & 0x1F);
		scount++;
	}

	pent->mouth.sndavg += savg;
	pent->mouth.sndcount = (byte)scount;

	if (pent->mouth.sndcount >= CAVGSAMPLES)
	{
		pent->mouth.mouthopen = pent->mouth.sndavg / CAVGSAMPLES;
		pent->mouth.sndavg = 0;
		pent->mouth.sndcount = 0;
	}
}

//===============================================================================
// VOX. Algorithms to load and play spoken text sentences from a file:
//
// In ambient sounds or entity sounds, precache the 
// name of the sentence instead of the wave name, ie: !C1A2S4
//
// During sound system init, the 'sentences.txt' is read.
// This file has the format:
//
//		HG_ALERT0 hgrunt/(t30) squad!, we!(e80) got!(e80) freeman!(t20 p105), clik(p110)
//      HG_ALERT1 hgrunt/clik(p110) target! clik
//		...
//
//		There must be at least one space between the sentence name and the sentence.
//		Sentences may be separated by one or more lines
//		There may be tabs or spaces preceding the sentence name
//		The sentence must end in a /n or /r
//		Lines beginning with // are ignored as comments
//
//		Period or comma will insert a pause in the wave unless
//		the period or comma is the last character in the string.
//
//		If first 2 chars of a word are upper case, word volume increased by 25%
// 
//		If last char of a word is a number from 0 to 9
//		then word will be pitch-shifted up by 0 to 9, where 0 is a small shift
//		and 9 is a very high pitch shift.
//
// We alloc heap space to contain this data, and track total 
// sentences read.  A pointer to each sentence is maintained in g_Sentences.
//
// When sound is played back in S_StartDynamicSound or s_startstaticsound, we detect the !name
// format and lookup the actual sentence in the sentences array
//
// To play, we parse each word in the sentence, chain the words, and play the sentence
// each word's data is loaded directy from disk and freed right after playback.
//===============================================================================

void VOX_Init( void )
{
	Q_memset(rgrgvoxword, 0, sizeof(rgrgvoxword));
	VOX_ReadSentenceFile();
}

// parse a null terminated string of text into component words, with
// pointers to each word stored in rgpparseword
// note: this code actually alters the passed in string!

void VOX_ParseString( char* psz )
{
	int i;
	bool bDone = false;
	char* pszscan = psz;
	char c;

	Q_memset(rgpparseword, 0, sizeof(rgpparseword));

	if (!psz)
		return;

	i = 0;
	rgpparseword[i++] = psz;

	while (!bDone && i < Q_ARRAYSIZE(rgpparseword))
	{
		// scan up to next word
		c = *pszscan;
		while (c && !(c == '.' || c == ' ' || c == ','))
			c = *(++pszscan);

		// if '(' then scan for matching ')'
		if (c == '(')
		{
			while (*pszscan != ')')
				pszscan++;

			c = *(++pszscan);
			if (!c)
				bDone = true;
		}

		if (bDone || !c)
			bDone = true;
		else
		{
			// if . or , insert pause into rgpparseword,
			// unless this is the last character
			if ((c == '.' || c == ',') && *(pszscan + 1) != '\n' && *(pszscan + 1) != '\r'
				&& *(pszscan + 1) != 0)
			{
				if (c == '.')
					rgpparseword[i++] = voxperiod;
				else
					rgpparseword[i++] = voxcomma;

				if (i >= Q_ARRAYSIZE(rgpparseword))
					break;
			}

			// null terminate substring
			*pszscan++ = 0;

			// skip whitespace
			c = *pszscan;
			while (c && (c == '.' || c == ' ' || c == ','))
				c = *(++pszscan);

			if (!c)
				bDone = true;
			else
				rgpparseword[i++] = pszscan;
		}
	}
}

// backwards scan psz for last '/'
// return substring in szpath null terminated
// if '/' not found, return 'vox/'

char* VOX_GetDirectory( char* szpath, char* psz, int nsize )
{
	char c;
	int cb = 0;
	char* pszscan = psz + Q_strlen(psz) - 1;

	// scan backwards until first '/' or start of string
	c = *pszscan;
	while (pszscan > psz && c != '/')
	{
		c = *(--pszscan);
		cb++;
	}

	if (c != '/')
	{
		// didn't find '/', return default directory
		Q_strcpy(szpath, "vox/");
		return psz;
	}

	cb = Q_strlen(psz) - cb;

	if (cb >= nsize)
	{
		Con_DPrintf("VOX_GetDirectory: invalid directory in: %s\n", psz);
		return nullptr;
	}

	Q_memcpy(szpath, psz, cb);
	szpath[cb] = 0;
	return pszscan + 1;
}

// set channel volume based on volume of current word
void VOX_SetChanVol( channel_t* ch )
{
	float scale; // [0..1] volume
	int vol;

	if (ch->isentence < 0)
		return;

	vol = rgrgvoxword[ch->isentence][ch->iword].volume;

	if (vol == 100)
		return;
	
	scale = (float)vol / 100.0f;

	if (scale == 1.0)
		return;

	ch->rightvol = (int)(ch->rightvol * scale);
	ch->leftvol = (int)(ch->leftvol * scale);
}

//===============================================================================
//  Get any pitch, volume, start, end params into voxword
//  and null out trailing format characters
//  Format: 
//		someword(v100 p110 s10 e20)
//		
//		v is volume, 0% to n%
//		p is pitch shift up 0% to n%
//		s is start wave offset %
//		e is end wave offset %
//		t is timecompression %
//
//	pass fFirst == 1 if this is the first string in sentence
//  returns 1 if valid string, 0 if parameter block only.
//
//  If a ( xxx ) parameter block does not directly follow a word, 
//  then that 'default' parameter block will be used as the default value
//  for all following words.  Default parameter values are reset
//  by another 'default' parameter block.  Default parameter values
//  for a single word are overridden for that word if it has a parameter block.
// 
//===============================================================================

int VOX_ParseWordParams( char* psz, voxword_t* pvoxword, int fFirst )
{
	static voxword_t voxwordDefault;

	char* pszsave = psz;
	char c;
	char ct;
	char sznum[8];
	int i;

	// init to defaults if this is the first word in string.
	if (fFirst)
	{
		voxwordDefault.pitch = -1;
		voxwordDefault.volume = 100;
		voxwordDefault.start = 0;
		voxwordDefault.end = 100;
		voxwordDefault.fKeepCached = 0;
		voxwordDefault.timecompress = 0;
	}

	*pvoxword = voxwordDefault;

	// look at next to last char to see if we have a 
	// valid format:

	c = *(psz + Q_strlen(psz) - 1);

	if (c != ')')
		return 1;		// no formatting, return

	// scan forward to first '('
	c = *psz;
	while (!(c == '(' || c == ')'))
		c = *(++psz);

	if (c == ')')
		return 0;		// bogus formatting

	// null terminate

	*psz = 0;
	ct = *(++psz);

	while (true)
	{
		// scan until we hit a character in the commandSet

		while (ct && !(ct == 'v' || ct == 'p' || ct == 's' || ct == 'e' || ct == 't'))
			ct = *(++psz);

		if (ct == ')')
			break;

		Q_memset(sznum, 0, sizeof(sznum));
		i = 0;

		c = *(++psz);

		if (!isdigit(c))
			break;

		// read number
		while (isdigit(c) && i < sizeof(sznum) - 1)
		{
			sznum[i++] = c;
			c = *(++psz);
		}

		// get value of number
		i = Q_atoi(sznum);

		switch (ct)
		{
			case 'v': pvoxword->volume = i; break;
			case 'p': pvoxword->pitch = i; break;
			case 's': pvoxword->start = i; break;
			case 'e': pvoxword->end = i; break;
			case 't': pvoxword->timecompress = i; break;
		}

		ct = c;
	}

	// if the string has zero length, this was an isolated
	// parameter block.  Set default voxword to these
	// values

	if (Q_strlen(pszsave) == 0)
	{
		voxwordDefault = *pvoxword;
		return 0;
	}
	else
		return 1;
}

int VOX_IFindEmptySentence( void )
{
	int k;

	for (k = 0; k < CBSENTENCENAME_MAX; k++)
	{
		if (!rgrgvoxword[k][0].sfx)
			return k;
	}

	Con_DPrintf("Sentence or Pitch shift ignored. > 16 playing!\n");
	return -1;
}

void VOX_MakeSingleWordSentence( channel_t* ch, int pitch )
{
	int k;
	voxword_t voxword;

	k = VOX_IFindEmptySentence();

	if (k < 0)
	{
		ch->pitch = PITCH_NORM;
		ch->isentence = -1;
		return;
	}
	
	voxword.volume = 100;
	voxword.pitch = PITCH_NORM;
	voxword.end = 100;
	voxword.sfx = ch->sfx;
	voxword.start = 0;
	voxword.samplefrac = 0;
	voxword.timecompress = 0;
	voxword.fKeepCached = 1;

	rgrgvoxword[k][0] = voxword;
	rgrgvoxword[k][1].sfx = nullptr;

	ch->pitch = pitch;
	ch->isentence = k;
	ch->iword = 0;
}

// link all sounds in sentence, start playing first word.
sfxcache_t* VOX_LoadSound( channel_t* pchan, char* pszin )
{
	char buffer[512];
	int i, j, k, cword;
	char pathbuffer[64];
	char szpath[32];

	char* psz = nullptr;
	sfxcache_t* sc = nullptr;

	voxword_t rgvoxword[CVOXWORDMAX];

	if (!pszin)
		return nullptr;

	Q_memset(rgvoxword, 0, sizeof(voxword_t) * CVOXWORDMAX);
	Q_memset(buffer, 0, sizeof(buffer));

	// lookup actual string in rgpszrawsentence, 
	// set pointer to string data

	psz = VOX_LookupString(pszin, nullptr);

	if (!psz)
	{
		Con_DPrintf("VOX_LoadSound: no sentence named %s\n", pszin);
		return nullptr;
	}

	// get directory from string, advance psz
	psz = VOX_GetDirectory(szpath, psz, CVOXWORDMAX);

	if (!psz)
	{
		Con_DPrintf("VOX_LoadSound: failed getting directory for %s\n", pszin);
		return nullptr;
	}

	if ((uint32)Q_strlen(psz) > sizeof(buffer) - 1)
	{
		Con_DPrintf("VOX_LoadSound: sentence is too long %s\n", psz);
		return nullptr;
	}

	// copy into buffer
	Q_strncpy(buffer, psz, sizeof(buffer) - 1);
	buffer[sizeof(buffer) - 1] = 0;

	// parse sentence (also inserts null terminators between words)

	VOX_ParseString(buffer);

	// for each word in the sentence, construct the filename,
	// lookup the sfx and save each pointer in a temp array	

	i = 0;
	cword = 0;

	while (rgpparseword[i])
	{
		// Get any pitch, volume, start, end params into voxword

		if (VOX_ParseWordParams(rgpparseword[i], &rgvoxword[cword], i == 0))
		{
			// this is a valid word (as opposed to a parameter block)
			snprintf(pathbuffer, sizeof(pathbuffer), "%s%s.wav", szpath, rgpparseword[i]);
			pathbuffer[sizeof(pathbuffer) - 1] = 0;

			if (Q_strlen(pathbuffer) >= sizeof(pathbuffer))
				continue;

			// find name, if already in cache, mark voxword
			// so we don't discard when word is done playing
			rgvoxword[cword].sfx = S_FindName(pathbuffer, &(rgvoxword[cword].fKeepCached));
			cword++;
		}
		i++;
	}

	k = VOX_IFindEmptySentence();
	if (k < 0)
		return nullptr;

	j = 0;
	while (rgvoxword[j].sfx != nullptr)
		rgrgvoxword[k][j] = rgvoxword[j++];

	rgrgvoxword[k][j].sfx = nullptr;

	pchan->isentence = k;
	pchan->iword = 0;
	pchan->sfx = rgrgvoxword[k][0].sfx;
	
	sc = S_LoadSound(rgvoxword[0].sfx, nullptr);
	
	if (!sc)
	{
		S_FreeChannel(pchan);
		return nullptr;
	}

	return sc;
}

int VOX_FPaintPitchChannelFrom8Offs( portable_samplepair_t* paintbuffer, channel_t* ch, sfxcache_t* sc, int count, int pitch, int timecompress, int offset )
{
	int i;
	int time;
	int offs;
	int sampfr;
	int smpls;
	int cbtrim;
	int fracstep;
	int samplecnt;
	int chunksize;
	int played;
	int playcount;
	int skipbytes;
	int cdata;

	float stepscale;

	int* lscale = nullptr, * rscale = nullptr;

	portable_samplepair_t* pb = &paintbuffer[offset];

	if (ch->isentence < 0)
		return 0;

	if (ch->leftvol > 255)
		ch->leftvol = 255;
	if (ch->rightvol > 255)
		ch->rightvol = 255;

	lscale = snd_scaletable[ch->leftvol >> 3];
	rscale = snd_scaletable[ch->rightvol >> 3];

	sampfr = rgrgvoxword[ch->isentence][ch->iword].samplefrac;
	cbtrim = rgrgvoxword[ch->isentence][ch->iword].cbtrim;

	stepscale = (float)pitch / 100.0f;
	fracstep = stepscale * 256.0f;

	offs = fracstep + sampfr;
	time = timecompress;
	samplecnt = sampfr >> 8;
	
	if (timecompress)
	{
		chunksize = cbtrim / 8;	
		skipbytes = chunksize * timecompress / 100;

		time = 0;

		if (count > 0)
		{
			while (samplecnt < cbtrim)
			{
				if (samplecnt % chunksize >= skipbytes || samplecnt < skipbytes)
				{
				begin:
					cdata = chunksize - (samplecnt % chunksize);
				}
				else
				{
					for (i = 0; time < count; i++)
					{
						if (samplecnt >= cbtrim)
							break;

						if (i >= 255)
							break;

						if (sc->data[samplecnt] <= 2)
							break;

						pb[time].left += lscale[sc->data[samplecnt]];
						pb[time].right += rscale[sc->data[samplecnt]];
						samplecnt = offs >> 8;
						time++;
						offs += fracstep;
					}

					if (time > PAINTBUFFER_SIZE)
						Con_DPrintf("timecompress scan forward: overwrote paintbuffer!");

					if (samplecnt >= cbtrim || time >= count)
						break;

					while (true)
					{
						if (samplecnt % chunksize < skipbytes)
						{
							smpls = skipbytes - samplecnt % chunksize;
							samplecnt += smpls;
							offs += smpls << 8;
						}

						if (samplecnt >= cbtrim)
							break;

						i = 0;

						while (i < 255 && sc->data[samplecnt] > 2U)
						{
							samplecnt = offs >> 8;
							offs += fracstep;
							
							i++;

							if (samplecnt >= cbtrim)
								goto done;
						}

						if (samplecnt >= cbtrim)
							break;

						if (samplecnt % chunksize >= skipbytes)
							goto begin;
					}
				}

			done:
				playcount = (cdata << 8) / fracstep;

				if (playcount > count - time)
					playcount = count - time;
				
				if (!playcount)
					playcount = 1; // we should have at least one sample

				if (time >= count)
					break;

				played = 0;

				while (played < playcount && samplecnt < cbtrim)
				{
					pb[time].left += lscale[sc->data[samplecnt]];
					pb[time].right += rscale[sc->data[samplecnt]];
					samplecnt = offs >> 8;
					played++;
					time++;
					offs += fracstep;

					if (time >= count)
						goto endpoint;
				}

				if (time >= count)
					break;
			}
		}
	}
	else
	{
		if (count > 0)
		{
			while (samplecnt < cbtrim)
			{
				pb[time].left += lscale[sc->data[samplecnt]];
				pb[time].right += rscale[sc->data[samplecnt]];
				samplecnt = offs >> 8;
				time++;
				offs += fracstep;

				if (time >= count)
					break;
			}
		}
	}

endpoint:
	rgrgvoxword[ch->isentence][ch->iword].samplefrac = offs - fracstep;
	ch->end += time + ch->pos - (offs >> 8);
	ch->pos = offs >> 8;

	return samplecnt >= cbtrim;
}

// Load sentence file into memory, insert null terminators to
// delimit sentence name/sentence pairs.  Keep pointer to each
// sentence name so we can search later.

void VOX_ReadSentenceFile( void )
{
	int size;
	int nFileLength;
	int nSentenceCount;

	char c;
	char* pch, * pchlast;

	char* pBuf = nullptr;

	const char* pSentenceData = nullptr;
	const char* pszSentenceValue = nullptr;

	VOX_Shutdown();

	pBuf = (char*)COM_LoadFileForMe(szsentences, &nFileLength);

	if (pBuf == nullptr)
	{
		Con_DPrintf("VOX_ReadSentenceFile: Couldn't load %s\n", "sound/sentences.txt");
		return;
	}

	pch = pBuf;
	pchlast = &pch[nFileLength];
	nSentenceCount = 0;

	while (pch < pchlast)
	{
		if (nSentenceCount >= CVOXFILESENTENCEMAX) // hit the limit
			break;

		// Only process this pass on sentences
		pSentenceData = nullptr;

		// skip newline, cr, tab, space

		c = *pch;
		while (pch < pchlast && (c == '\n' || c == '\r' || c == '\t' || c == ' '))
			c = *(++pch);

		// skip entire line if first char is /
		if (*pch != '/')
		{
			pSentenceData = pch;

			// scan forward to first space, insert null terminator
			// after sentence name

			c = *pch;
			while (pch < pchlast && c != '\t' && c != ' ')
				c = *(++pch);

			if (pch < pchlast)
				*pch++ = 0;

			// A sentence may have some line commands, make an extra pass
			pszSentenceValue = pch;
		}
		// scan forward to end of sentence or eof
		while (pch < pchlast && pch[0] != '\n' && pch[0] != '\r')
			pch++;

		// insert null terminator
		if (pch < pchlast)
			*pch++ = 0;
		
		// If we have some sentence data
		if (pSentenceData)
		{
			size = (strlen(pszSentenceValue) + 1) + (strlen(pSentenceData) + 1);
			rgpszrawsentence[nSentenceCount] = (char*)Mem_Malloc(size);

			Q_memcpy(rgpszrawsentence[nSentenceCount], pSentenceData, size);
			nSentenceCount++;
		}
	};

	cszrawsentences = nSentenceCount;
	Mem_Free(pBuf);
}

void VOX_Shutdown( void )
{
	int i;

	for (i = 0; i < cszrawsentences; i++)
	{
		Mem_Free(rgpszrawsentence[i]);
	}

	cszrawsentences = 0;
}

// scan rgpszrawsentence, looking for pszin sentence name
// return pointer to sentence data if found, null if not
// CONSIDER: if we have a large number of sentences, should
// CONSIDER: sort strings in g_Sentences and do binary search.
char* VOX_LookupString( char* pszin, int* psentencenum )
{
	int i;
	char* cptr = nullptr;
	sentenceEntry_s* sentenceEntry = nullptr;

	if (pszin[0] == '#')
	{
		//sentenceEntry = SequenceGetSentenceByIndex(atoi(pszin + 1)); TODO: Implement

		if (sentenceEntry)
			return sentenceEntry->data;
	}

	for (i = 0; i < cszrawsentences; i++)
	{
		if (!Q_strcasecmp(pszin, rgpszrawsentence[i]))
		{
			if (psentencenum)
				*psentencenum = i;

			cptr = &rgpszrawsentence[i][Q_strlen(rgpszrawsentence[i]) + 1];
			while (*cptr == ' ' || *cptr == '\t')
				cptr++;

			return cptr;
		}
	}

	return nullptr;
}

void VOX_TrimStartEndTimes( channel_t* ch, sfxcache_t* sc )
{
	int length;
	int srcsample;
	int i, skiplen;

	float start, end;

	signed char* pdata = nullptr;
	voxword_t* pvoxword = nullptr;

	if (ch->isentence < 0)
		return;

	pvoxword = &rgrgvoxword[ch->isentence][ch->iword];
	pvoxword->cbtrim = sc->length;

	start = (float)pvoxword->start;
	end = (float)pvoxword->end;

	length = sc->length;
	pdata = (signed char*)sc->data;

	if (start >= end)
		return;

	if (start > 0 && start < 100)
	{
		skiplen = length * (start / 100);
		srcsample = ch->pos;
		ch->pos += skiplen;

		if (ch->pos < length)
		{
			for (i = 0; i < 255; i++)
			{
				if (srcsample >= length)
					break;

				if (pdata[srcsample] >= -2 && pdata[srcsample] <= 2)
				{
					ch->pos += i;
					ch->end -= skiplen + i;
					break;
				}

				srcsample++;
			}
		}

		if (pvoxword->pitch != PITCH_NORM)
			pvoxword->samplefrac += ch->pos << 8;
	}

	if (end > 0 && end < 100)
	{
		skiplen = sc->length * ((100 - end) / 100);
		length -= skiplen;
		srcsample = length;
		ch->end -= skiplen;

		if (ch->pos < length)
		{
			for (i = 0; i < 255 && srcsample; i++)
			{
				if (srcsample <= ch->pos)
					break;

				if (pdata[srcsample] >= -2 && pdata[srcsample] <= 2)
				{
					ch->end -= i;
					pvoxword->cbtrim -= skiplen + i;
					break;
				}

				srcsample--;
			}
		}
	}
}