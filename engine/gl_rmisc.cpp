#include "quakedef.h"
#include "qgl.h"
#include "render.h"
#include "gl_rmisc.h"

qboolean filterMode;
float filterColorRed = 1.0, filterColorGreen = 1.0, filterColorBlue = 1.0;
float filterBrightness = 1.0;

cvar_t gl_clear = { "gl_clear", "0" };

void R_InitTextures()
{
	//TODO: implement - Solokiller
}

void R_Init()
{
	//TODO: implement - Solokiller
}

void D_FlushCaches()
{
	//Nothing
}
