#include "quakedef.h"
#include "client.h"
#include "dll_state.h"
#include "host_cmd.h"
#include "server.h"
#include "cd.h"
#include <vgui_int.h>
#include <gl_screen.h>

bool s_careerAudioPaused = false;

CareerStateType g_careerState = CAREER_NONE;

bool g_iQuitCommandIssued = false;

bool g_bMajorMapChange = false;

void Host_InitializeGameDLL()
{
	Cbuf_Execute();
	NET_Config((qboolean)svs.maxclients > 1);

	if (svs.dll_initialized)
	{
		Con_DPrintf("Sys_InitializeGameDLL called twice, skipping second call\n");
		return;
	}

	svs.dll_initialized = true;
	LoadEntityDLLs(host_parms.basedir);
	gEntityInterface.pfnGameInit();
	gEntityInterface.pfnPM_Init( &g_svmove );
	gEntityInterface.pfnRegisterEncoders();

	/* - TODO: implement - ScriptedSnark
	SV_InitEncoders();
	SV_GetPlayerHulls();
	SV_CheckBlendingInterface();
	SV_CheckSaveGameCommentInterface();
	*/
	Cbuf_Execute();
}


void Host_ClearSaveDirectory()
{
	//TODO: implement - Solokiller
}

void Host_Maps_f()
{
	if (Cmd_Argc() != 2)
	{
		Con_Printf("Usage:  maps <substring>\nmaps * for full listing\n");
	}

	const char* pszSubString = (char*)Cmd_Argv(1);

	if (pszSubString && *pszSubString)
	{
		if (*pszSubString == '*')
			pszSubString = nullptr;

		COM_ListMaps(pszSubString);
	}
}

void Host_Quit_f()
{
	if (Cmd_Argc() == 1)
	{
		giActive = DLL_CLOSE;
		g_iQuitCommandIssued = true;
		//TODO: implement - Solokiller
		/*
		if( cls.state != ca_dedicated )
			CL_Disconnect();
		Host_ShutdownServer( 0 );
		*/
		Sys_Quit();
	}
	else
	{
		giActive = DLL_PAUSED;
		giStateInfo = 4;
	}
}

void Host_Quit_Restart_f()
{
	giActive = DLL_STATE_RESTART;
	giStateInfo = 4;

	/* TODO: implement after reversing game world/server/client  - THIS CODE IS DECOMPILED, NEEDED TO CORRECT - ScriptedSnark
	if ( sv.active
    || cls.state == ca_active
    && cls.trueaddress[0]
    && g_pPostRestartCmdLineArgs
    && (strcat(g_pPostRestartCmdLineArgs, " +connect "), strcat(g_pPostRestartCmdLineArgs, cls.servername), sv.active) )
	{
		if ( svs.maxclients == 1 && cls.state == ca_active )
		{
			if ( g_pPostRestartCmdLineArgs )
			{
				Cbuf_AddText("save quick\n");
				Cbuf_Execute();
				strcat(g_pPostRestartCmdLineArgs, " +load quick");
			}
    }
	*/
}

void Host_InitCommands()
{
	if (!g_bIsDedicatedServer)
	{
		Cmd_AddCommand("cd", CD_Command_f);
		Cmd_AddCommand("mp3", MP3_Command_f);
		//Cmd_AddCommand("_careeraudio", CareerAudio_Command_f); TODO: Implement
	}

	//TODO: implement - Solokiller
	Cmd_AddCommand("quit", Host_Quit_f);
	Cmd_AddCommand("exit", Host_Quit_f);
	Cmd_AddCommand("maps", Host_Maps_f);
	//TODO: implement - Solokiller
	Cmd_AddCommand("_restart", Host_Quit_Restart_f);
	Cmd_AddCommand("_sethdmodels", Host_SetHDModels_f);
	Cmd_AddCommand("_setaddons_folder", Host_SetAddonsFolder_f);
	Cmd_AddCommand("_set_vid_level", Host_SetVideoLevel_f);
	//TODO: implement - Solokiller
}
