#include "quakedef.h"
#include "host.h"
#include "gl_draw.h"

sizebuf_t net_message;

extern cvar_t cl_showfps;

void NET_Config( bool multiplayer )
{
	//TODO: implement - Solokiller
}

void NET_Init()
{
	//TODO: implement - Solokiller
}

void NET_Shutdown()
{
	//TODO: implement - Solokiller
}

void NET_DrawString( int x, int y, int font, float r, float g, float b, char* fmt, ... )
{
	static char string[1024];
	va_list varargs;

	va_start(varargs, fmt);
	vsnprintf(string, sizeof(string), fmt, varargs);
	va_end(varargs);

	Draw_SetTextColor(r, g, b);
	Draw_String(x, y, string);
}

/*
==================
SCR_NetGraph

Visualizes data flow
==================
*/
void SCR_NetGraph( void )
{
	// TODO: Implement
}

static double rolling_fps;

void SCR_DrawFPS( void )
{
	if (!cl_showfps.value)
		return;

	if (host_frametime <= 0.0)
		return;

	rolling_fps = 0.6 * rolling_fps + host_frametime * 0.4;
	NET_DrawString(2, 2, 0, 1, 1, 1, "%d fps", (int)floor(1.0 / rolling_fps));
}