#include "quakedef.h"
#include "qgl.h"
#include "gl_rmain.h"

int currenttexture = -1;	// to avoid unnecessary texture sets

int cnttextures[ 2 ] = { -1, -1 };     // cached

vec3_t r_origin = { 0, 0, 0 };

refdef_t r_refdef = {};

int isFogEnabled = 0;

model_t* R_LoadMapSprite( const char *szFilename )
{
	//TODO: implement - Solokiller
	return nullptr;
}

void AllowFog( bool allowed )
{
    if (allowed)
    {
        if (isFogEnabled == 1)
            qglEnable(GL_FOG);
    }
    else
    {
        isFogEnabled = qglIsEnabled(GL_FOG);
        if (isFogEnabled == 1)
            qglDisable(GL_FOG);
    }
}
