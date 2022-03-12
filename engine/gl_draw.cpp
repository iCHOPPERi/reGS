#include "quakedef.h"
#include "cdll_int.h"
#include "decals.h"
#include "gl_draw.h"
#include "gl_rmain.h"
#include "gl_screen.h"
#include "gl_vidnt.h"
#include "qgl.h"
#include "wad.h"

#include "vgui2/text_draw.h"

cvar_t gl_ansio = { "gl_ansio", "16" };

qpic_t* draw_disc = nullptr;

typedef struct
{
	int texnum;
	__int16 servercount;
	__int16 paletteIndex;
	int width;
	int height;
	qboolean mipmap;
	char identifier[64];
} gltexture_t;

static gltexture_t	gltextures[MAX_GLTEXTURES];

struct GL_PALETTE
{
	int tag;
	int referenceCount;
	unsigned __int8 colors[768];
};

GL_PALETTE gGLPalette[350];
static int numgltextures = 0;

void Draw_Init()
{
	m_bDrawInitialized = true;
	VGUI2_Draw_Init();

	//TODO: implement - Solokiller

	Cvar_RegisterVariable( &gl_ansio );

	if( Host_GetVideoLevel() > 0 )
	{
		Cvar_DirectSet( &gl_ansio, "4" );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_ansio.value );
	}
	//TODO: implement - Solokiller
}

void Draw_FillRGBA( int x, int y, int w, int h, int r, int g, int b, int a )
{
	//qglDisable(1);
}

int Draw_Character( int x, int y, int num, unsigned int font )
{
	return VGUI2_Draw_Character( x, y, num, font );
}

int Draw_MessageCharacterAdd( int x, int y, int num, int rr, int gg, int bb, unsigned int font )
{
	return VGUI2_Draw_CharacterAdd( x, y, num, rr, gg, bb, font );
}

int Draw_String( int x, int y, char* str )
{
	int width = VGUI2_DrawString(x, y, str, VGUI2_GetConsoleFont());
	Draw_ResetTextColor();

	return width + x;
}

int Draw_StringLen( const char* psz, unsigned int font )
{
	return VGUI2_Draw_StringLen( psz, font );
}

void Draw_SetTextColor( float r, float g, float b )
{
	g_engdstAddrs.pfnDrawSetTextColor( &r, &g, &b );

	VGUI2_Draw_SetTextColor(
		static_cast<int>( r * 255.0 ),
		static_cast<int>( g * 255.0 ),
		static_cast<int>( b * 255.0 )
	);
}

void Draw_GetDefaultColor()
{
	int r, g, b;

	if( sscanf( con_color.string, "%i %i %i", &r, &g, &b ) == 3 )
		VGUI2_Draw_SetTextColor( r, g, b );
}

void Draw_ResetTextColor()
{
	Draw_GetDefaultColor();
}

void Draw_FillRGBABlend( int x, int y, int w, int h, int r, int g, int b, int a )
{
	//TODO: implement - Solokiller
}

GLuint GL_GenTexture()
{
	GLuint tex;

	qglGenTextures( 1, &tex );

	return tex;
}

GLenum oldtarget = TEXTURE0_SGIS;

void GL_UnloadTexture(char* identifier)
{/* --------- THIS CODE IS REVERSED BY GHIDRA | TODO: NORMALIZE THE CODE - ScriptedSnark ---------
	int iVar1;
	uint uVar2;
	gltexture_t* dest;
	int iVar3;

	if (numgltextures > 0) {
		dest = gltextures;
		iVar3 = 0;
		do {
			iVar1 = Q_strcmp(identifier, dest->identifier);
			if (iVar1 == 0) {
				if ((*(byte*)((int)&dest->servercount + 1) & 128) != 0) {
					return;
				}
				(*qglDeleteTextures)(1, (GLuint*)dest);
				uVar2 = (uint)dest->paletteIndex;
				if (-1 < (int)uVar2) {
					if (gGLPalette[uVar2].referenceCount < 2) {
						if (uVar2 < 350) {
							gGLPalette[uVar2].tag = -1;
							gGLPalette[uVar2].referenceCount = 0;
						}
					}
					else {
						gGLPalette[uVar2].referenceCount = gGLPalette[uVar2].referenceCount + -1;
					}
				}
				Q_memset(dest, 0, 84);
				dest->servercount = -1;
				return;
			}
			iVar3 = iVar3 + 1;
			dest = dest + 1;
		} while (iVar3 < numgltextures);
	}
	return;
	*/
}

void GL_SelectTexture( GLenum target )
{
	if( !gl_mtexable )
		return;

	qglSelectTextureSGIS( target );

	if( target == oldtarget )
		return;

	cnttextures[ oldtarget - TEXTURE0_SGIS ] = currenttexture;
	currenttexture = cnttextures[ target - TEXTURE0_SGIS ];
	oldtarget = target;
}

void Draw_Pic( int x, int y, qpic_t* pic )
{
	//TODO: implement - Solokiller
}

void Draw_BeginDisc()
{
	if( draw_disc )
		Draw_CenterPic( draw_disc );
}
