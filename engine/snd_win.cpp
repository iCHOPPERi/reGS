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

// Reverse-Engineered by Doomsayer
// https://github.com/1Doomsayer

#define CINTERFACE

#include "quakedef.h"
#include "sound.h"

#include "winsani_in.h"
#include <Windows.h>
#include "winsani_out.h"

#include <SDL2/SDL_syswm.h>
#include <mmeapi.h>
#include <mmsystem.h>
#include <dsound.h>

#define iDirectSoundCreate(a,b,c)	pDirectSoundCreate(a,b,c)

HRESULT(WINAPI* pDirectSoundCreate)(GUID FAR* lpGUID, LPDIRECTSOUND FAR* lplpDS, IUnknown FAR* pUnkOuter);

// 64K is > 1 second at 16-bit, 22050 Hz
#define	WAV_BUFFERS             64
#define	WAV_MASK				(WAV_BUFFERS - 1)
#define	WAV_BUFFER_SIZE			0x0400
#define SECONDARY_BUFFER_SIZE	0x10000

extern SDL_Window* pmainwindow;

enum sndinitstat
{
	SIS_SUCCESS,
	SIS_FAILURE,
	SIS_NOTAVAIL
};

static qboolean wavonly = false;
static qboolean dsound_init = false;
static qboolean wav_init = false;
static qboolean snd_firsttime = true, snd_isdirect, snd_iswave;
static qboolean primary_format_set = false;

static int snd_inited;

static int sample16;
static int snd_sent, snd_completed;


/*
*	Global variables. Must be visible to window-procedure function
*	so it can unlock and free the data block after it has been played.
*/

HANDLE		hData;
HPSTR		lpData, lpData2;

HGLOBAL		hWaveHdr;
LPWAVEHDR	lpWaveHdr;

HWAVEOUT    hWaveOut;

WAVEOUTCAPS	wavecaps;

DWORD	gSndBufSize;

MMTIME		mmstarttime;

LPDIRECTSOUND pDS;
LPDIRECTSOUNDBUFFER pDSBuf, pDSPBuf;

HINSTANCE hInstDS;

bool g_fUseDInput = false; // Use direct input?

extern int snd_blocked;

void SetMouseEnable( qboolean fEnable )
{
}

void Snd_ReleaseBuffer( void )
{
	if (snd_isdirect)
	{
		if (!--snd_inited)
		{
			S_ClearBuffer();
			S_Shutdown();
		}
	}
}

void Snd_AcquireBuffer( void )
{
	if (snd_isdirect)
	{
		if (++snd_inited == 1)
			S_Startup();
	}
}

/*
==================
S_BlockSound
==================
*/
void S_BlockSound( void )
{
	// DirectSound takes care of blocking itself
	if (snd_iswave)
	{
		snd_blocked++;

		if (snd_blocked == 1)
			waveOutReset(hWaveOut);
	}
}

/*
==================
S_UnblockSound
==================
*/
void S_UnblockSound( void )
{
	// DirectSound takes care of blocking itself
	if (snd_iswave)
	{
		snd_blocked--;
	}
}

/*
==================
FreeSound
==================
*/
void FreeSound( void )
{
	int	i;

	if (pDSBuf)
	{
		pDSBuf->lpVtbl->Stop(pDSBuf);
		pDSBuf->lpVtbl->Release(pDSBuf);
	}

	// only release primary buffer if it's not also the mixing buffer we just released
	if (pDSPBuf && (pDSBuf != pDSPBuf))
	{
		pDSPBuf->lpVtbl->Release(pDSPBuf);
	}

	if (pDS)
	{
		SDL_SysWMinfo wmInfo;
		SDL_VERSION(&wmInfo.version);
		SDL_GetWindowWMInfo((SDL_Window*)pmainwindow, &wmInfo);

		pDS->lpVtbl->SetCooperativeLevel(pDS, wmInfo.info.win.window, DSSCL_NORMAL);
		pDS->lpVtbl->Release(pDS);
	}

	if (hWaveOut)
	{
		waveOutReset(hWaveOut);

		if (lpWaveHdr)
		{
			for (i = 0; i < WAV_BUFFERS; i++)
				waveOutUnprepareHeader(hWaveOut, &lpWaveHdr[i], sizeof(WAVEHDR));
		}

		waveOutClose(hWaveOut);
		hWaveOut = 0;

		if (hWaveHdr)
		{
			GlobalUnlock(hWaveHdr);
			GlobalFree(hWaveHdr);
		}

		if (hData)
		{
			GlobalUnlock(hData);
			GlobalFree(hData);
		}
	}

	pDS = NULL;
	pDSBuf = NULL;
	pDSPBuf = NULL;
	hWaveOut = 0;
	hData = 0;
	hWaveHdr = 0;
	lpData = NULL;
	lpWaveHdr = NULL;
	dsound_init = false;
	wav_init = false;

	if (hInstDS)
	{
		pDirectSoundCreate = NULL;
		FreeLibrary(hInstDS);
		hInstDS = NULL;
	}
}

/*
==================
SNDDMA_InitDirect

Direct-Sound support
==================
*/
sndinitstat SNDDMA_InitDirect( void )
{
	SDL_SysWMinfo	wmInfo;

	DSBCAPS			dsbcaps;
	DWORD			dwSize, dwWrite;
	DSBUFFERDESC	dsbuf;
	DSCAPS			dscaps;
	WAVEFORMATEX	format, pformat;
	HRESULT			hresult;
	int				reps;

	Q_memset((void*)&sn, 0, sizeof(sn));

	shm = &sn;

	shm->channels = 2;
	shm->samplebits = 16;
	shm->speed = 11025;
	shm->dmaspeed = SOUND_DMA_SPEED;

	Q_memset(&format, 0, sizeof(format));
	format.wFormatTag = WAVE_FORMAT_PCM;
	format.nChannels = shm->channels;
	format.wBitsPerSample = shm->samplebits;
	format.nSamplesPerSec = shm->dmaspeed;
	format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8;
	format.cbSize = 0;
	format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;

	if (!hInstDS)
	{
		hInstDS = (HINSTANCE)FS_LoadLibrary("dsound.dll");

		if (!hInstDS)
		{
			Con_SafePrintf("Couldn't load dsound.dll\n");
			return SIS_FAILURE;
		}

		pDirectSoundCreate = (decltype(pDirectSoundCreate))GetProcAddress(hInstDS, "DirectSoundCreate");

		if (!pDirectSoundCreate)
		{
			Con_SafePrintf("Couldn't get DS proc addr\n");
			return SIS_FAILURE;
		}
	}

	if (iDirectSoundCreate(NULL, &pDS, NULL) != DS_OK)
	{
		Con_DPrintf("DirectSound create failed.\n");
		return SIS_FAILURE;
	}

	dscaps.dwSize = sizeof(dscaps);

	if (DS_OK != pDS->lpVtbl->GetCaps(pDS, &dscaps))
	{
		Con_SafePrintf("Couldn't get DS caps\n");
	}

	if (dscaps.dwFlags & DSCAPS_EMULDRIVER)
	{
		Con_SafePrintf("No DirectSound driver installed\n");
		FreeSound();
		return SIS_FAILURE;
	}

	if (pmainwindow)
	{
		SDL_VERSION(&wmInfo.version);
		SDL_GetWindowWMInfo((SDL_Window*)pmainwindow, &wmInfo);

		if (DS_OK != pDS->lpVtbl->SetCooperativeLevel(pDS, wmInfo.info.win.window, DSSCL_EXCLUSIVE))
		{
			Con_SafePrintf("Set coop level failed\n");
			FreeSound();
			return SIS_FAILURE;
		}
	}

	// get access to the primary buffer, if possible, so we can set the
	// sound hardware format
	Q_memset(&dsbuf, 0, sizeof(dsbuf));
	dsbuf.dwSize = sizeof(DSBUFFERDESC);
	dsbuf.dwFlags = (DSBCAPS_PRIMARYBUFFER | DSBCAPS_CTRL3D | DSBCAPS_CTRLVOLUME);
	dsbuf.dwBufferBytes = 0;
	dsbuf.lpwfxFormat = NULL;

	Q_memset(&dsbcaps, 0, sizeof(dsbcaps));
	dsbcaps.dwSize = sizeof(dsbcaps);
	primary_format_set = false;

	if (!COM_CheckParm("-snoforceformat"))
	{
		if (DS_OK == pDS->lpVtbl->CreateSoundBuffer(pDS, &dsbuf, &pDSPBuf, NULL))
		{
			pformat = format;

			if (DS_OK != pDSPBuf->lpVtbl->SetFormat(pDSPBuf, &pformat))
			{
				if (snd_firsttime)
					Con_DPrintf("Set primary sound buffer format: no\n");
			}
			else
			{
				if (snd_firsttime)
					Con_DPrintf("Set primary sound buffer format: yes\n");

				primary_format_set = true;
			}
		}
	}

	if (!primary_format_set || !COM_CheckParm("-primarysound"))
	{
	// create the secondary buffer we'll actually work with
		Q_memset(&dsbuf, 0, sizeof(dsbuf));
		dsbuf.dwSize = sizeof(DSBUFFERDESC);
		dsbuf.dwFlags = DSBCAPS_CTRLFREQUENCY | DSBCAPS_LOCSOFTWARE;
		dsbuf.dwBufferBytes = SECONDARY_BUFFER_SIZE;
		dsbuf.lpwfxFormat = &format;

		Q_memset(&dsbcaps, 0, sizeof(dsbcaps));
		dsbcaps.dwSize = sizeof(dsbcaps);

		if (DS_OK != pDS->lpVtbl->CreateSoundBuffer(pDS, &dsbuf, &pDSBuf, NULL))
		{
			Con_SafePrintf("DS:CreateSoundBuffer Failed");
			FreeSound();
			return SIS_FAILURE;
		}

		shm->channels = format.nChannels;
		shm->samplebits = format.wBitsPerSample;
		shm->dmaspeed = format.nSamplesPerSec;

		if (DS_OK != pDSBuf->lpVtbl->GetCaps(pDSBuf, &dsbcaps))
		{
			Con_SafePrintf("DS:GetCaps failed\n");
			FreeSound();
			return SIS_FAILURE;
		}

		if (snd_firsttime)
			Con_SafePrintf("Using secondary sound buffer\n");
	}
	else
	{
		SDL_VERSION(&wmInfo.version);
		SDL_GetWindowWMInfo((SDL_Window*)pmainwindow, &wmInfo);

		if (DS_OK != pDS->lpVtbl->SetCooperativeLevel(pDS, wmInfo.info.win.window, DSSCL_WRITEPRIMARY))
		{
			Con_SafePrintf("Set coop level failed\n");
			FreeSound();
			return SIS_FAILURE;
		}

		if (DS_OK != pDSPBuf->lpVtbl->GetCaps(pDSPBuf, &dsbcaps))
		{
			Con_Printf("DS:GetCaps failed\n");
			return SIS_FAILURE;
		}

		pDSBuf = pDSPBuf;
		Con_SafePrintf("Using primary sound buffer\n");
	}

	// Make sure mixer is active
	if (DS_OK != pDSBuf->lpVtbl->Play(pDSBuf, 0, 0, DSBPLAY_LOOPING))
		Con_DPrintf("Failed to start play\n");

	if (snd_firsttime)
		Con_DPrintf("   %d channel(s)\n"
			"   %d bits/sample\n"
			"   %d bytes/sec\n",
			shm->channels, shm->samplebits, shm->speed);

	gSndBufSize = dsbcaps.dwBufferBytes;

// initialize the buffer
	reps = 0;

	while ((hresult = pDSBuf->lpVtbl->Lock(pDSBuf, 0, gSndBufSize, (LPVOID*)&lpData, &dwSize, NULL, NULL, 0)) != DS_OK)
	{
		if (hresult != DSERR_BUFFERLOST)
		{
			Con_SafePrintf("SNDDMA_InitDirect: DS::Lock Sound Buffer Failed\n");
			FreeSound();
			return SIS_FAILURE;
		}

		if (++reps > 10000)
		{
			Con_SafePrintf("SNDDMA_InitDirect: DS: couldn't restore buffer\n");
			FreeSound();
			return SIS_FAILURE;
		}
	}

	Q_memset(lpData, 0, dwSize);
//		lpData[4] = lpData[5] = 0x7f;	// force a pop for debugging

	pDSBuf->lpVtbl->Unlock(pDSBuf, lpData, dwSize, NULL, 0);

	/* we don't want anyone to access the buffer directly w/o locking it first. */
	lpData = NULL;

	pDSBuf->lpVtbl->Stop(pDSBuf);
	pDSBuf->lpVtbl->GetCurrentPosition(pDSBuf, &mmstarttime.u.sample, &dwWrite);
	pDSBuf->lpVtbl->Play(pDSBuf, 0, 0, DSBPLAY_LOOPING);

	shm->soundalive = true;
	shm->splitbuffer = false;
	shm->samples = gSndBufSize / (shm->samplebits / 8);
	shm->samplepos = 0;
	shm->submission_chunk = 1;
	shm->buffer = (unsigned char*)lpData;
	sample16 = (shm->samplebits / 8) - 1;

	dsound_init = true;

	return SIS_SUCCESS;
}

/*
==================
SNDDM_InitWav

Crappy windows multimedia base
==================
*/
qboolean SNDDMA_InitWav( void )
{
	WAVEFORMATEX  format;
	int				i;
	HRESULT			hr;

	snd_sent = 0;
	snd_completed = 0;

	shm = &sn;

	shm->channels = 2;
	shm->samplebits = 16;
	shm->speed = 11025;
	shm->dmaspeed = SOUND_DMA_SPEED;

	Q_memset(&format, 0, sizeof(format));
	format.wFormatTag = WAVE_FORMAT_PCM;
	format.nChannels = shm->channels;
	format.wBitsPerSample = shm->samplebits;
	format.nSamplesPerSec = shm->dmaspeed;
	format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8;
	format.cbSize = 0;
	format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;

	if (hr = waveOutOpen(&hWaveOut, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
	{
		Con_Printf("The sound hardware is in use by another app.\nSound not available ( %i ).\n", hr);
		return false;
	}

	/*
	*	Allocate and lock memory for the waveform data. The memory
	*	for waveform data must be globally allocated with
	*	GMEM_MOVEABLE and GMEM_SHARE flags.
	*/
	gSndBufSize = WAV_BUFFERS * WAV_BUFFER_SIZE;
	hData = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE, gSndBufSize);
	
	if (!hData)
	{
		Con_SafePrintf("Sound: Out of memory.\n");
		FreeSound();
		return false;
	}

	lpData = (HPSTR)GlobalLock(hData);

	if (!lpData)
	{
		Con_SafePrintf("Sound: Failed to lock.\n");
		FreeSound();
		return false;
	}

	Q_memset(lpData, 0, gSndBufSize);

	/*
	*	Allocate and lock memory for the header. This memory must
	*	also be globally allocated with GMEM_MOVEABLE and
	*	GMEM_SHARE flags.
	*/
	hWaveHdr = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE, (DWORD)sizeof(WAVEHDR) * WAV_BUFFERS);

	if (hWaveHdr == NULL)
	{
		Con_SafePrintf("Sound: Failed to Alloc header.\n");
		FreeSound();
		return false;
	}

	lpWaveHdr = (LPWAVEHDR)GlobalLock(hWaveHdr);

	if (lpWaveHdr == NULL)
	{
		Con_SafePrintf("Sound: Failed to lock header.\n");
		FreeSound();
		return false;
	}

	Q_memset(lpWaveHdr, 0, sizeof(WAVEHDR) * WAV_BUFFERS);

	/* After allocation, set up and prepare headers. */
	for (i = 0; i < WAV_BUFFERS; i++)
	{
		lpWaveHdr[i].dwBufferLength = WAV_BUFFER_SIZE;
		lpWaveHdr[i].lpData = lpData + i * WAV_BUFFER_SIZE;

		if (waveOutPrepareHeader(hWaveOut, lpWaveHdr + i, sizeof(WAVEHDR)) !=
			MMSYSERR_NOERROR)
		{
			Con_SafePrintf("Sound: failed to prepare wave headers\n");
			FreeSound();
			return false;
		}
	}

	shm->soundalive = true;
	shm->splitbuffer = false;
	shm->samples = gSndBufSize / (shm->samplebits / 8);
	shm->samplepos = 0;
	shm->submission_chunk = 1;
	shm->buffer = (byte*)lpData;
	sample16 = (shm->samplebits / 8) - 1;

	wav_init = true;

	return true;
}

extern bool Win32AtLeastV4; // TODO: Add this extern in winquake.h -Doomsayer

/*
==================
SNDDMA_Init

Try to find a sound device to mix for.
Returns false if nothing is found.
==================
*/
qboolean SNDDMA_Init( void )
{
	sndinitstat	stat;

	if (COM_CheckParm("-wavonly"))
		wavonly = true;

	dsound_init = wav_init = false;

	stat = SIS_FAILURE;	// assume DirectSound won't initialize

	/* Init DirectSound */
	if (!wavonly && Win32AtLeastV4)
	{
		if (snd_firsttime || snd_isdirect)
		{
			stat = SNDDMA_InitDirect();

			if (stat == SIS_SUCCESS)
			{
				snd_isdirect = true;

				if (snd_firsttime)
					Con_DPrintf("DirectSound initialized\n");
			}
			else
			{
				snd_isdirect = false;
				Con_DPrintf("DirectSound failed to init\n");
			}
		}
	}

	// if DirectSound didn't succeed in initializing, try to initialize
	// waveOut sound, unless DirectSound failed because the hardware is
	// already allocated (in which case the user has already chosen not
	// to have sound)
	if (!dsound_init && (stat != SIS_NOTAVAIL))
	{
		if (snd_firsttime || snd_iswave)
		{
			snd_iswave = SNDDMA_InitWav();

			if (snd_iswave)
			{
				if (snd_firsttime)
					Con_DPrintf("Wave sound initialized\n");
			}
			else
			{
				Con_DPrintf("Wave sound failed to init\n");
			}
		}
	}

	snd_firsttime = false;
	snd_inited = true;

	return dsound_init || wav_init;
}

/*
==============
SNDDMA_GetDMAPos

return the current sample position (in mono samples read)
inside the recirculating dma buffer, so the mixing code will know
how many sample are required to fill it up.
===============
*/
int SNDDMA_GetDMAPos( void )
{
	MMTIME	mmtime;
	int		s = 0;
	DWORD	dwWrite = 0;

	if (dsound_init)
	{
		mmtime.wType = TIME_SAMPLES;
		pDSBuf->lpVtbl->GetCurrentPosition(pDSBuf, &mmtime.u.sample, &dwWrite);
		s = mmtime.u.sample - mmstarttime.u.sample;
	}
	else if (wav_init)
	{
		s = snd_sent * WAV_BUFFER_SIZE;
	}

	s >>= sample16;

	s &= (shm->samples - 1);

	return s;
}

/*
==============
SNDDMA_Submit

Send sound to device if buffer isn't really the dma buffer
===============
*/
void SNDDMA_Submit( void )
{
	LPWAVEHDR	h;
	int			wResult;

	if (!wav_init)
		return;

	//
	// find which sound blocks have completed
	//
	while (true)
	{
		if (snd_completed == snd_sent)
		{
			Con_DPrintf("Sound overrun\n");
			break;
		}

		if (!(lpWaveHdr[snd_completed & WAV_MASK].dwFlags & WHDR_DONE))
		{
			break;
		}

		snd_completed++;	// this buffer has been played
	}

	//
	// submit two new sound blocks
	//
	while (((snd_sent - snd_completed) >> sample16) < 8)
	{
		h = &lpWaveHdr[snd_sent & WAV_MASK];

		snd_sent++;

		/*
		*	Now the data block can be sent to the output device. The
		*	waveOutWrite function returns immediately and waveform
		*	data is sent to the output device in the background.
		*/
		wResult = waveOutWrite(hWaveOut, h, sizeof(WAVEHDR));

		if (wResult != MMSYSERR_NOERROR)
		{
			Con_SafePrintf("Failed to write block to device\n");
			FreeSound();
			return;
		}
	}
}

/*
==============
SNDDMA_Shutdown

Reset the sound device for exiting
===============
*/
void SNDDMA_Shutdown( void )
{
	FreeSound();
}
