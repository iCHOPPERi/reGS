/**
*	@file
*
	portable code to mix sounds for snd_dma.cpp
*/

#include "quakedef.h"
#include "sound.h"

extern "C"
{
	extern int	snd_scaletable[32][256];
};

int			snd_scaletable[32][256];

void VOX_Init()
{
	//TODO: implement - Solokiller
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
