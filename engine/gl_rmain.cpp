#include "quakedef.h"
#include "qgl.h"
#include "gl_rmain.h"
#include "gl_rmisc.h"

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

float CalcFov(float fov_x, float width, float height) // TODO: improve the code - ScriptedSnark
{
    long double fov; // fst7
    float x; // [esp+0h] [ebp-2Ch]

    if (fov_x < 1.0)
    {
        fov = 0.9999999999999999;
    }
    else if (fov_x <= 179.0)
    {
        fov = tan(fov_x / 360.0 * M_PI);
    }
    else
    {
        fov = 0.9999999999999999;
    }
    x = height / (width / fov);

    return atanf(x) * 360.0 / M_PI;
}

void R_ForceCVars(qboolean mp)
{
    if (mp)
    {
        // TODO: implement - ScriptedSnark
        D_FlushCaches();
    }
}