/**
*	@file
*
	portable code to mix sounds for snd_dma.cpp
*/

#include "quakedef.h"
#include "sound.h"

#define SND_SCALE_BITS		7
#define SND_SCALE_SHIFT		(8-SND_SCALE_BITS)
#define SND_SCALE_LEVELS	(1<<SND_SCALE_BITS)

extern "C"
{
	extern int	snd_scaletable[SND_SCALE_LEVELS][256];
};

void VOX_Init()
{
	//TODO: implement - Solokiller
}

void SND_InitScaletable()
{
    int        i, j;

    for (i = 0; i < SND_SCALE_LEVELS; i++)
        for (j = 0; j < 256; j++)
            snd_scaletable[i][j] = ((signed char)j) * i * (1 << SND_SCALE_SHIFT);
}
