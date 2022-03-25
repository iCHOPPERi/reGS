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

// snd_mem.cpp: sound caching

#include "quakedef.h"
#include "sound.h"
#include "client.h" // TODO: Should be included in quakedef.h -Doomsayer
//#include "voice_sound_engine_interface.h" TODO: Implement

extern wavstream_t wavstreams[MAX_CHANNELS];

sfxcache_t* S_LoadStreamSound( sfx_t* s, channel_t* ch );
bool GetWavinfo( char* name, byte* wav, int wavlength, wavinfo_t* info );

/*
================
ResampleSfx
================
*/
void ResampleSfx( sfx_t* sfx, int inrate, int inwidth, byte* data, int datasize )
{
	int	i;
	int	outcount;
	int	srcsample;
	int	sample, samplefrac, fracstep;

	float stepscale = 1.0f;

	sfxcache_t* sc = nullptr;

	sc = (sfxcache_t*)Cache_Check(&sfx->cache);

	if (!sc)
		return;

	if (shm)
	{
		stepscale = (float)inrate / (float)shm->speed;	// this is usually 0.5, 1, or 2

		if (stepscale == 2 && hisound.value != 0.0)
		{
			outcount = sc->length;
			stepscale = 1.0;
			goto skip;
		}
	}

	outcount = sc->length / stepscale;
	sc->length = outcount;
	if (sc->loopstart != -1)
		sc->loopstart = sc->loopstart / stepscale;

	if (shm)
		sc->speed = shm->speed;

skip:
	if (loadas8bit.value)
		sc->width = 1;	
	else
		sc->width = inwidth;

	sc->stereo = 0;

	// resample / decimate to the current source rate

	if (stepscale == 1.0 && inwidth == 1 && sc->width == 1)
	{
		// fast special case
		if (outcount > datasize)
		{
			Con_DPrintf("ResampleSfx: %s has invalid sample count: %d datasize: %d\n", sfx->name, outcount, datasize);
			memset(sc->data, 0, outcount);
			return;
		}
		for (i = 0; i < outcount; i++)
			sc->data[i] = data[i] + 128;
	}
	else
	{
		// general case
		if (stepscale != 1.0 && stepscale != 2.0)
			Con_DPrintf("WARNING! %s is causing runtime sample conversion!\n", sfx->name);

		samplefrac = 0;
		fracstep = stepscale * 256;

		if (outcount > 0)
		{
			if ((fracstep * (outcount - 1)) > 0x7FFFFFFF || (fracstep * (outcount - 1)) >> 8 >= datasize)
			{
				Con_DPrintf("ResampleSfx: %s has invalid resample count: %d step: %d datasize: %d\n", sfx->name, outcount, fracstep, datasize);
				memset(sc->data, 0, outcount);
				return;
			}
		}

		for (i = 0; i < outcount; i++)
		{
			srcsample = samplefrac >> 8;
			samplefrac += fracstep;

			if (inwidth == 2)
				sample = LittleShort(((short*)data)[srcsample]);
			else
				sample = (int)((byte)(data[srcsample]) - 128) << 8;
			
			if (sc->width == 2)
				((short*)sc->data)[i] = sample;
			else
				((byte*)sc->data)[i] = sample >> 8;
		}
	}
}

//=============================================================================

/*
==============
S_LoadSound
==============
*/
sfxcache_t* S_LoadSound( sfx_t* s, channel_t* ch )
{
	float len;
	float stepscale;
	double startTime;

	byte stackbuf[1024];
	char namebuffer[256];

	int size;
	int filesize;
	wavinfo_t info;

	FileHandle_t hFile = FILESYSTEM_INVALID_HANDLE;

	byte* data = nullptr;
	sfxcache_t* sc = nullptr;

	if (s->name[0] == '*')
		return S_LoadStreamSound(s, ch);

	//if (s->name[0] == '?') TODO: Implement
		//return VoiceSE_GetSFXCache(s);

	sc = (sfxcache_t*)Cache_Check(&s->cache);
	if (sc)
	{
		if (hisound.value != 0.0 || !shm || sc->speed <= shm->speed)
			return sc;
		Cache_Free(&s->cache);
	}

	if (fs_precache_timings.value != 0.0)
		startTime = Sys_FloatTime();

	Q_strcpy(namebuffer, "sound");

	if (s->name[0] != '/')
		strncat(namebuffer, "/", sizeof(namebuffer) - 1 - strlen(namebuffer));
	strncat(namebuffer, s->name, sizeof(namebuffer) - 1 - strlen(namebuffer));

	hFile = FS_Open(namebuffer, "rb");

	if (!hFile)
	{
		namebuffer[0] = '\0';

		if (s->name[0] != '/')
			strncat(namebuffer, "/", sizeof(namebuffer) - 1 - strlen(namebuffer));

		strncat(namebuffer, s->name, sizeof(namebuffer) - 1 - strlen(namebuffer));
		namebuffer[sizeof(namebuffer) - 1] = 0;

		data = COM_LoadStackFile(namebuffer, stackbuf, 1024, &filesize);

		if (!data)
		{
			Con_DPrintf("S_LoadSound: Couldn't load %s\n", namebuffer);
			return nullptr;
		}

		hFile = 0;
	}
	else
	{
		data = (byte*)FS_GetReadBuffer(hFile, &filesize);

		if (!data)
		{
			filesize = FS_Size(hFile);
			data = (byte*)Hunk_TempAlloc(filesize + 1);

			FS_Read(data, filesize, hFile);
			FS_Close(hFile);

			if (!data)
			{
				namebuffer[0] = '\0';

				if (s->name[0] != '/')
					strncat(namebuffer, "/", sizeof(namebuffer) - 1 - strlen(namebuffer));

				strncat(namebuffer, s->name, sizeof(namebuffer) - 1 - strlen(namebuffer));
				namebuffer[sizeof(namebuffer) - 1] = 0;

				data = COM_LoadStackFile(namebuffer, stackbuf, 1024, &filesize);

				if (!data)
				{
					Con_DPrintf("S_LoadSound: Couldn't load %s\n", namebuffer);
					return nullptr;
				}
			}

			hFile = 0;
		}
	}

	if (!GetWavinfo(s->name, data, filesize, &info))
	{
		Con_DPrintf("Failed loading %s\n", s->name);
		return nullptr;
	}

	if (info.channels != 1)
	{
		Con_DPrintf("%s is a stereo sample\n", s->name);
		return nullptr;
	}

	if (!info.rate)
	{
		Con_DPrintf("Invalid rate: %s\n", s->name);
		return nullptr;
	}

	if (info.dataofs >= filesize)
	{
		Con_DPrintf("%s has invalid data offset\n", s->name);
		return nullptr;
	}

	stepscale = 1.f;

	if ((shm && info.rate != shm->speed) && info.rate != 2 * shm->speed || hisound.value <= 0.0)
	{
		stepscale = (float)info.rate / (float)shm->speed;

		if (stepscale == 0.0)
		{
			Con_DPrintf("Invalid stepscale: %s\n", s->name);
			return nullptr;
		}
	}

	size = 24;
	len = (float)info.samples / stepscale;

	if (len)
	{
		if (0x7FFFFFFF / len < 1)
		{
			Con_DPrintf("Invalid length (s/c): %s\n", s->name);
			return nullptr;
		}

		if (0x7FFFFFFF / len < info.width)
		{
			Con_DPrintf("Invalid length (s/c/w): %s\n", s->name);
			return nullptr;
		}

		if ((uint32)(0x7FFFFFFF - len * info.width) < 24)
			Con_DPrintf("Invalid length (s/c/w/sfx): %s\n", s->name);

		size = len * info.width + 24;
	}

	sc = (sfxcache_t*)Cache_Alloc(&s->cache, size, s->name);

	if (sc)
	{
		sc->length = info.samples;
		sc->loopstart = info.loopstart;
		sc->speed = info.rate;
		sc->width = info.width;
		sc->stereo = info.channels;

		ResampleSfx(s, sc->speed, sc->width, data + info.dataofs, filesize - info.dataofs);

		if (hFile)
		{
			FS_ReleaseReadBuffer(hFile, data);
			FS_Close(hFile);
		}

		if (fs_precache_timings.value != 0.0)
		{
			// Print the loading time
			Con_DPrintf("fs_precache_timings: loaded sound %s in time %.3f sec\n", namebuffer, Sys_FloatTime() - startTime);
		}
	}

	return sc;
}

/*
==============
S_LoadStreamSound
==============
*/
sfxcache_t* S_LoadStreamSound( sfx_t* s, channel_t* ch )
{
	int cbread;
	uint32 stream;
	char wavname[64];
	char namebuffer[256];

	int smpc, size;
	wavinfo_t info;

	FileHandle_t hFile = FILESYSTEM_INVALID_HANDLE;

	byte* data = nullptr;
	sfxcache_t* sc = nullptr;

	if (cl.fPrecaching)
		return nullptr;

	Q_strncpy(wavname, &s->name[1], sizeof(wavname) - 1);
	wavname[sizeof(wavname) - 1] = 0;

	stream = (uint32)((char*)ch - (char*)channels) >> 6;

	sc = (sfxcache_t*)Cache_Check(&s->cache);
	if (sc)
	{
		if (wavstreams[stream].hFile)
			return sc;
	}

	Q_strcpy(namebuffer, "sound");

	if (wavname[0] != '/')
		strncat(namebuffer, "/", 255 - strlen(namebuffer));
	strncat(namebuffer, wavname, 255 - strlen(namebuffer));

	hFile = wavstreams[stream].hFile;
	data = COM_LoadFileLimit(namebuffer, wavstreams[stream].lastposloaded, 0x8000, &cbread, &wavstreams[stream].hFile);
	if (!data)
	{
		Con_DPrintf("S_LoadStreamSound: Couldn't load %s\n", namebuffer);
		return nullptr;
	}

	if (!hFile)
	{
		if (!GetWavinfo(s->name, data, cbread, &info))
		{
			Con_DPrintf("Failed loading %s\n", wavname);
			return nullptr;
		}

		if (info.channels != 1)
		{
			Con_DPrintf("%s is a stereo sample\n", wavname);
			return nullptr;
		}

		if (info.width != 1)
		{
			Con_DPrintf("%s is a 16 bit sample\n", wavname);
			return nullptr;
		}

		if (shm && info.rate != shm->speed)
		{
			Con_DPrintf("%s ignored, not stored at playback sample rate!\n", wavname);
			return nullptr;
		}

		wavstreams[stream].csamplesinmem = cbread - info.dataofs;
	}
	else
	{
		memcpy(&info, &wavstreams[stream].info, sizeof(info));
		wavstreams[stream].csamplesinmem = cbread;
	}

	smpc = info.samples - wavstreams[stream].csamplesplayed;
	if (wavstreams[stream].csamplesinmem > smpc)
		wavstreams[stream].csamplesinmem = smpc;

	memcpy(&wavstreams[stream].info, &info, sizeof(info));

	if (!sc)
	{
		size = cbread;

		if (size > 0x8000)
			size = 0x8000;

		sc = (sfxcache_t*)Cache_Alloc(&s->cache, size + 24, wavname);
		
		if (!sc)
			return sc;
	}

	sc->length = wavstreams[stream].csamplesinmem;
	sc->loopstart = info.loopstart;
	sc->speed = info.rate;
	sc->width = info.width;
	sc->stereo = info.channels;

	if (!hFile)
		ResampleSfx(s, sc->speed, sc->width, &data[info.dataofs], cbread - info.dataofs);
	else
		ResampleSfx(s, sc->speed, sc->width, data, cbread);

	return sc;
}

/*
===============================================================================

WAV loading

===============================================================================
*/


byte* data_p;
byte* iff_end;
byte* last_chunk;
byte* iff_data;
int 	iff_chunk_len;


short GetLittleShort( void )
{
	short val = 0;
	val = *data_p;
	val = val + (*(data_p + 1) << 8);
	data_p += 2;
	return val;
}

int GetLittleLong( void )
{
	int val = 0;
	val = *data_p;
	val = val + (*(data_p + 1) << 8);
	val = val + (*(data_p + 2) << 16);
	val = val + (*(data_p + 3) << 24);
	data_p += 4;
	return val;
}

void FindNextChunk( char* name )
{
	while (true)
	{
		data_p = last_chunk;

		if (last_chunk >= iff_end)
		{
			data_p = nullptr;
			return;
		}

		data_p += 4;
		iff_chunk_len = GetLittleLong();

		if (iff_chunk_len < 0 || !strcmpi(name, "LIST") && &data_p[iff_chunk_len] > iff_end)
		{
			data_p = nullptr;
			return;
		}

		if (iff_chunk_len > 0x6400000)
		{
			data_p = nullptr;
			return;
		}

		last_chunk = &data_p[(iff_chunk_len + 1) & 0xFFFFFFFE];
		data_p -= 8;

		if (!Q_strncmp((const char*)data_p, name, 4))
			return;
	}
}

void FindChunk( char* name )
{
	last_chunk = iff_data;
	FindNextChunk(name);
}


void DumpChunks( void )
{
	char	str[5];

	str[4] = 0;
	data_p = iff_data;
	do
	{
		Q_memcpy(str, data_p, 4);
		data_p += 4;
		iff_chunk_len = GetLittleLong();
		Con_Printf("0x%x : %s (%d)\n", (int)(data_p - 4), str, iff_chunk_len);
		data_p += (iff_chunk_len + 1) & ~1;
	} while (data_p < iff_end);
}

/*
============
GetWavinfo
============
*/
bool GetWavinfo( char* name, byte* wav, int wavlength, wavinfo_t* info )
{
	int	samples;
	short format;

	if (!info || !wav)
		return false;

	Q_memset(info, 0, sizeof(wavinfo_t));

	iff_data = wav;
	iff_end = &wav[wavlength];

	// find "RIFF" chunk
	FindChunk("RIFF");

	if (!data_p || Q_strncmp((const char*)(data_p + 8), "WAVE", 4))
	{
		Con_Printf("Missing RIFF/WAVE chunks\n");
		return false;
	}

	// get "fmt " chunk
	iff_data = data_p + 12;
	// DumpChunks ();

	FindChunk("fmt ");

	if (data_p == nullptr)
	{
		Con_Printf("Missing fmt chunk\n");
		return false;
	}

	data_p += 8;
	format = GetLittleShort();

	if (format != 1)
	{
		Con_Printf("Microsoft PCM format only\n");
		return false;
	}

	info->channels = GetLittleShort();
	info->rate = GetLittleLong();

	data_p += 4 + 2;

	info->width = GetLittleShort() / 8;

	if (!info->width)
	{
		Con_Printf("Invalid sample width\n");
		return false;
	}

	// get cue chunk
	FindChunk("cue ");

	if (data_p)
	{
		data_p += 32;
		info->loopstart = GetLittleLong();

		// if the next chunk is a LIST chunk, look for a cue length marker
		FindNextChunk("LIST");

		if (data_p && !Q_strncmp((const char*)(data_p + 28), "mark", 4))
		{
			data_p += 24;
			info->samples = info->loopstart + GetLittleLong();
		}
	}
	else
		info->loopstart = -1;

	// find data chunk
	FindChunk("data");

	if (data_p == nullptr)
	{
		Con_Printf("Missing data chunk\n");
		return false;
	}

	data_p += 4;

	samples = GetLittleLong() / info->width;

	if (info->samples)
	{
		if (samples < info->samples)
			Sys_Error("Sound %s has a bad loop length", name);
	}
	else
		info->samples = samples;

	info->dataofs = data_p - wav;

	if (info->channels < 0 || info->rate < 0 || info->width < 0 || info->samples < 0)
	{
		Con_Printf("Invalid wav info: %s\n", name);
		return false;
	}

	return true;
}
