#include "quakedef.h"
#include "r_triangle.h"
#include "GL/glew.h"
#include "qgl.h"
#include "vgui2/text_draw.h"
#include "cl_tent.h"
#include "gl_draw.h"

GLfloat flFogDensity;
bool g_bFogSkybox;
int gRenderMode;

int g_GL_Modes[7] // TODO: make all these integers as constants in QGL
{
	4,
	6,
	7,
	9,
	1,
	5,
	8
};

float gGlR, gGlG, gGlB, gGlW;

triangleapi_t tri =
{
	TRI_API_VERSION,
	&tri_GL_RenderMode,
	&tri_GL_Begin,
	&tri_GL_End,
	&tri_GL_Color4f,
	&tri_GL_Color4ub,
	&tri_GL_TexCoord2f,
	&tri_GL_Vertex3fv,
	&tri_GL_Vertex3f,
	&tri_GL_Brightness,
	&tri_GL_CullFace,
	&R_TriangleSpriteTexture,
	&tri_ScreenTransform,
	&R_RenderFog,
	&tri_WorldTransform,
	&tri_GetMatrix,
	&tri_BoxinPVS,
	&tri_LightAtPoint,
	&tri_GL_Color4fRendermode,
	&R_FogParams
};

void tri_GL_RenderMode( int mode )
{
	switch (mode)
	{
	case 0:
		qglDisable(GL_BLEND);
		qglDepthMask(GL_TRUE);
		qglShadeModel(GL_FLAT);
		gRenderMode = mode;
	case 1:
	case 2:
		qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		qglEnable(GL_BLEND);
		qglShadeModel(GL_SMOOTH);
		gRenderMode = mode;
		break;
	case 4:
		qglEnable(GL_BLEND);
		qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		qglShadeModel(GL_SMOOTH);
		qglDepthMask(GL_FALSE);
		gRenderMode = mode;
		break;
	case 5:
		qglBlendFunc(GL_ONE, GL_ONE);
		qglEnable(GL_BLEND);
		qglDepthMask(GL_FALSE);
		qglShadeModel(GL_SMOOTH);
		gRenderMode = mode;
		break;
	default:
		gRenderMode = mode;
		break;
	}
}

void tri_GL_Begin( int primitiveCode )
{
	VGUI2_ResetCurrentTexture();
	qglBegin(g_GL_Modes[primitiveCode]);
}

void tri_GL_End()
{
	qglEnd();
}

void tri_GL_Color4f( float x, float y, float z, float w )
{
	//TODO: implement - Solokiller
}

void tri_GL_Color4ub( byte r, byte g, byte b, byte a )
{
	//TODO: implement - Solokiller
}

void tri_GL_TexCoord2f( float u, float v )
{
	qglTexCoord2f(u, v);
}

void tri_GL_Vertex3fv( float* worldPnt )
{
	//TODO: implement - Solokiller
}

void tri_GL_Vertex3f( float x, float y, float z )
{
	//TODO: implement - Solokiller
}

void tri_GL_Brightness( float x )
{
	//TODO: implement - Solokiller
}

void tri_GL_CullFace( TRICULLSTYLE style )
{
	if (style)
	{
		if (style == TRI_NONE)
			qglDisable(GL_CULL_FACE);
	}
	else
	{
		qglEnable(GL_CULL_FACE);
		qglCullFace(GL_FRONT);
	}
}

int R_TriangleSpriteTexture( model_t *pSpriteModel, int frame )
{
	mspriteframe_t* sprframe = R_GetSpriteFrame((msprite_t*)pSpriteModel->cache.data, frame); // eax

	VGUI2_ResetCurrentTexture();
	
	if (!sprframe)
		return 0;

	if (sprframe)
	{
		GL_Bind(sprframe->gl_texturenum);
		return 1;
	}

	return 0;
}

int tri_ScreenTransform( float* world, float* screen )
{
	//TODO: implement - Solokiller
	return false;
}

void R_RenderFog( float* flFogColor, float flStart, float flEnd, int bOn )
{
	//TODO: implement - Solokiller
}

void tri_WorldTransform( float* screen, float* world )
{
	//TODO: implement - Solokiller
}

void tri_GetMatrix( const int pname, float* matrix )
{
	qglGetFloatv(pname, matrix);
}

int tri_BoxinPVS( float* mins, float* maxs )
{
	//TODO: implement - Solokiller
	return false;
}

void tri_LightAtPoint( float* vPos, float* value )
{
	//TODO: implement - Solokiller
}

void tri_GL_Color4fRendermode( float x, float y, float z, float w, int rendermode )
{
	//TODO: implement - Solokiller
}

void R_FogParams( float flDensity, int iFogSkybox )
{
	flFogDensity = flDensity;
	g_bFogSkybox = iFogSkybox;
}
