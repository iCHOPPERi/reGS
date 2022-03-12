#ifndef ENGINE_SND_H
#define ENGINE_SND_H

// sound engine rate defines
#define SOUND_DMA_SPEED		22050		// hardware playback rate
#define SOUND_11k			11025		// 11khz sample rate
#define SOUND_22k			22050		// 22khz sample rate
#define SOUND_44k			44100		// 44khz sample rate
#define SOUND_ALL_RATES		1			// mix all sample rates

#define SOUND_MIX_WET		0			// mix only samples that don't have channel set to 'dry' (default)
#define SOUND_MIX_DRY		1			// mix only samples with channel set to 'dry' (ie: music)

#define	SOUND_BUSS_ROOM			(1<<0)		// mix samples using channel dspmix value (based on distance from player)
#define SOUND_BUSS_FACING		(1<<1)		// mix samples using channel dspface value (source facing)
#define	SOUND_BUSS_FACINGAWAY	(1<<2)		// mix samples using 1-dspface

typedef struct sfx_s
{
	char 	name[ MAX_QPATH ];
	cache_user_t	cache;
	int servercount;
} sfx_t;

typedef struct dma_s
{
    qboolean gamealive;
    qboolean soundalive;
    qboolean splitbuffer;
    int channels;
    int samples;
    int submission_chunk;
    int samplepos;
    int samplebits;
    int speed;
    int dmaspeed;
    unsigned char* buffer;
} dma_t;

extern cvar_t suitvolume;

extern bool g_fUseDInput;

void S_Init();

void S_Shutdown();

sfx_t* S_PrecacheSound( const char* name );

void S_StartDynamicSound( int entnum, int entchannel, sfx_t* sfx, vec3_t origin, float fvol, float attenuation, int flags, int pitch );

void S_StopSound( int entnum, int entchannel );

void S_StopAllSounds( bool clear );

void Snd_AcquireBuffer();
void Snd_ReleaseBuffer();
void SND_InitScaletable(void);
int SNDDMA_Init(void);

void SetMouseEnable( int fState );

void VOX_Init();

#endif //ENGINE_SND_H
