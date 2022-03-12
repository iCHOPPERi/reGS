#include <ctime>

#include "quakedef.h"
#include "buildnum.h"
#include "cdaudio.h"
#include "cdll_int.h"
#include "chase.h"
#include "cl_main.h"
#include "cl_parsefn.h"
#include "client.h"
#include "cmodel.h"
#include "decals.h"
#include "delta.h"
#include "DemoPlayerWrapper.h"
#include "dll_state.h"
#include "gl_draw.h"
#include "gl_model.h"
#include "gl_rmisc.h"
#include "gl_screen.h"
#include "hashpak.h"
#include "host.h"
#include "host_cmd.h"
#include "net_chan.h"
#include "pmove.h"
#include "qgl.h"
#include "server.h"
#include "sound.h"
#include "sv_main.h"
#include "sv_upld.h"
#include "SystemWrapper.h"
#include "vgui_int.h"
#include "view.h"
#include "voice.h"
#include "wad.h"

quakeparms_t host_parms = {};

jmp_buf host_abortserver;
jmp_buf host_enddemo;

bool host_initialized = false;
double realtime = 0;
double oldrealtime = 0;
double host_frametime = 0;

client_t* host_client = nullptr;

cvar_t console = { "console", "0.0", FCVAR_ARCHIVE };

static cvar_t host_profile = { "host_profile", "0" };

cvar_t fps_max = { "fps_max", "100.0", FCVAR_ARCHIVE };
cvar_t fps_override = { "fps_override", "0" };

cvar_t host_framerate = { "host_framerate", "0" };

cvar_t sys_ticrate = { "sys_ticrate", "100.0" };
cvar_t sys_timescale = { "sys_timescale", "1.0" };

cvar_t developer = { "developer", "0" };

unsigned short* host_basepal = nullptr;

int host_hunklevel = 0;

void Host_InitLocal()
{
	Host_InitCommands();

	//Cvar_RegisterVariable(&host_killtime); // TODO: Implement
	Cvar_RegisterVariable(&sys_ticrate);
	Cvar_RegisterVariable(&fps_max);
	Cvar_RegisterVariable(&fps_override);
	//Cvar_RegisterVariable(&host_name); // TODO: Implement
	//Cvar_RegisterVariable(&host_limitlocal); // TODO: Implement
	sys_timescale.value = 1;
	Cvar_RegisterVariable(&host_framerate);
	//Cvar_RegisterVariable( &host_speeds );
	Cvar_RegisterVariable(&host_profile);

	Cvar_RegisterVariable(&mp_logfile);
	Cvar_RegisterVariable(&mp_logecho);
	Cvar_RegisterVariable(&sv_log_onefile);
	Cvar_RegisterVariable(&sv_log_singleplayer);
	Cvar_RegisterVariable(&sv_logsecret);
	//TODO: implement - Solokiller
	/*
	Cvar_RegisterVariable( &sv_stats );
	*/
	Cvar_RegisterVariable( &developer );
	//TODO: implement - Solokiller
	/*
	Cvar_RegisterVariable( &deathmatch );
	Cvar_RegisterVariable( &coop );
	Cvar_RegisterVariable( &pausable );
	Cvar_RegisterVariable( &skill );
	*/

	SV_SetMaxclients();
}

void Host_WriteConfiguration()
{
	//TODO: implement - Solokiller
}

void Host_Error( const char* error, ... )
{
	static bool inerror = false;

	char string[ 1024 ];
	va_list va;

	va_start( va, error );

	if( inerror == false )
	{
		inerror = true;

		//TODO: implement - Solokiller
		//SCR_EndLoadingPlaque();

		vsnprintf( string, ARRAYSIZE( string ), error, va );

		//TODO: implement - Solokiller
		/*
		if( !( _DWORD ) sv.active && developer.value != 0.0 )
			CL_WriteMessageHistory( 0, 0 );
			*/

		Con_Printf( "Host_Error: %s\n", string );

		//TODO: implement - Solokiller
		/*
		if( ( _DWORD ) sv.active )
			Host_ShutdownServer( 0 );

		if( ( _DWORD ) cls.state )
		{
			CL_Disconnect();
			cls.demonum = -1;
			inerror = false;
			longjmp( host_abortserver, 1 );
		}
		*/

		Sys_Error( "Host_Error: %s\n", string );
	}

	va_end( va );

	Sys_Error( "Host_Error: recursively entered" );
}

void Host_UpdateSounds(void)
{
	if (cls.state == ca_dedicated)
		return;

	// update audio
	if (cls.state == ca_active)
	{
		// TODO: implement h_origin, h_forward, h_right, h_up - ScriptedSnark
		// S_Update(h_origin, h_forward, h_right, h_up);
	}
	else
	{
		S_Update(vec3_origin, vec3_origin, vec3_origin, vec3_origin);
	}
}

void CheckGore()
{
	char szBuffer[ 128 ];

	Q_memset( szBuffer, 0, sizeof( szBuffer ) );

	//TODO: needs Windows specific code - Solokiller

	if( bLowViolenceBuild )
	{
		Cvar_SetValue( "violence_hblood", 0 );
		Cvar_SetValue( "violence_hgibs", 0 );
		Cvar_SetValue( "violence_ablood", 0 );
		Cvar_SetValue( "violence_agibs", 0 );
	}
	else
	{
		Cvar_SetValue( "violence_hblood", 1 );
		Cvar_SetValue( "violence_hgibs", 1 );
		Cvar_SetValue( "violence_ablood", 1 );
		Cvar_SetValue( "violence_agibs", 1 );
	}
}

void Host_Version()
{
	Q_strcpy( gpszVersionString, "1.0.1.4" );
	Q_strcpy( gpszProductString, "valve" );

	char szFileName[ FILENAME_MAX ];

	strcpy( szFileName, "steam.inf" );

	FileHandle_t hFile = FS_Open( szFileName, "r" );

	if( hFile != FILESYSTEM_INVALID_HANDLE )
	{
		const int iSize = FS_Size( hFile );
		void* pFileData = Mem_Malloc( iSize + 1 );
		FS_Read( pFileData, iSize, hFile );
		FS_Close( hFile );

		char* pBuffer = reinterpret_cast<char*>( pFileData );

		pBuffer[ iSize ] = '\0';

		const int iProductNameLength = Q_strlen( "ProductName=" );
		const int iPatchVerLength = Q_strlen( "PatchVersion=" );

		char szSteamVersionId[ 32 ];

		//Parse out the version and name.
		for( int i = 0; ( pBuffer = COM_Parse( pBuffer ) ) != nullptr && *com_token && i < 2; )
		{
			if( !Q_strnicmp( com_token, "PatchVersion=", iPatchVerLength ) )
			{
				++i;

				Q_strncpy( gpszVersionString, &com_token[ iPatchVerLength ], ARRAYSIZE( gpszVersionString ) );

				if( COM_CheckParm( "-steam" ) )
				{
					FS_GetInterfaceVersion( szSteamVersionId, ARRAYSIZE( szSteamVersionId ) - 1 );
					snprintf( gpszVersionString, ARRAYSIZE( gpszVersionString ), "%s/%s", &com_token[ iPatchVerLength ], szSteamVersionId );
				}
			}
			else if( !Q_strnicmp( com_token, "ProductName=", iProductNameLength ) )
			{
				++i;
				Q_strncpy( gpszProductString, &com_token[ iProductNameLength ], ARRAYSIZE( gpszProductString ) );
			}
		}

		if( pFileData )
			Mem_Free( pFileData );
	}

	Con_Printf( "Protocol version %i\nExe version %s (%s)\n", PROTOCOL_VERSION, gpszVersionString, gpszProductString );
	Con_Printf( "Exe build: " __TIME__ " " __DATE__ " (%i)\n", build_number() );
}

bool Host_Init( quakeparms_t* parms )
{
	srand( time( nullptr ) );

	host_parms = *parms;

	realtime = 0;

	com_argc = parms->argc;
	com_argv = parms->argv;

	Memory_Init( parms->membase, parms->memsize );

	Voice_RegisterCvars();
	Cvar_RegisterVariable( &console );

	//if( COM_CheckParm( "-console" ) || COM_CheckParm( "-toconsole" ) || COM_CheckParm( "-dev" ) )
	Cvar_DirectSet( &console, "1.0" );

	Cmd_AddMallocCommand("version", Host_Version, 2);

	Host_InitLocal();

	if( COM_CheckParm( "-dev" ) )
		Cvar_SetValue( "developer", 1.0 );

	Cbuf_Init();
	Cmd_Init();
	Cvar_Init();
	Cvar_CmdInit();

	V_Init();
	Chase_Init();

	COM_Init();
	Host_ClearSaveDirectory();
	HPAK_Init();

	W_LoadWadFile( "gfx.wad" );
	W_LoadWadFile( "fonts.wad" );

	Key_Init();
	Con_Init();
	Decal_Init();
	Mod_Init();
	NET_Init();
	Netchan_Init();
	DELTA_Init();
	SV_Init();
	SystemWrapper_Init();
	Host_Version();

	char versionString[ 256 ];
	snprintf( versionString, ARRAYSIZE( versionString ), "%s,%i,%i", gpszVersionString, PROTOCOL_VERSION, build_number() );

	Cvar_Set( "sv_version", versionString );

	Con_DPrintf( "%4.1f Mb heap\n", parms->memsize / (1024 * 1024 ) );

	R_InitTextures();
	HPAK_CheckIntegrity( "custom" );

	Q_memset( &g_module, 0, sizeof( g_module ) );

	if( cls.state != ca_dedicated )
	{
		byte* pPalette = COM_LoadHunkFile( "gfx/palette.lmp" );

		if( !pPalette )
			Sys_Error( "Host_Init: Couldn't load gfx/palette.lmp" );

		byte* pSource = pPalette;

		//Convert the palette from BGR to RGBA. TODO: these are the right formats, right? - Solokiller
		host_basepal = reinterpret_cast<unsigned short*>( Hunk_AllocName( 4 * 256 * sizeof( unsigned short ), "palette.lmp" ) );

		for( int i = 0; i < 256; ++i, pSource += 3 )
		{
			host_basepal[ ( 4 * i ) ] = *( pSource + 2 );
			host_basepal[ ( 4 * i ) + 1 ] = *( pSource + 1 );
			host_basepal[ ( 4 * i ) + 2 ] = *pSource;
			host_basepal[ ( 4 * i ) + 3 ] = 0;
		}

		GL_Init();
		PM_Init( &g_clmove );
		CL_InitEventSystem();
		ClientDLL_Init();
		VGui_Startup();

		if( !VID_Init( host_basepal ) )
		{
			VGui_Shutdown();
			return false;
		}

		Draw_Init();
		SCR_Init();
		R_Init();
		S_Init();
		CDAudio_Init();
		Voice_Init( "voice_speex", 1 );
		DemoPlayer_Init();
		CL_Init();
	}
	else
	{
		Cvar_RegisterVariable( &suitvolume );
	}

	Cbuf_InsertText( "exec valve.rc\n" );

	if( cls.state != ca_dedicated )
		GL_Config();

	Hunk_AllocName( 0, "-HOST_HUNKLEVEL-" );
	host_hunklevel = Hunk_LowMark();

	giActive = DLL_ACTIVE;
	scr_skipupdate = false;

	CheckGore();

	host_initialized = true;

	return true;
}

void Host_Shutdown()
{
	static bool isdown = false;

	if( isdown )
	{
		puts( "recursive shutdown" );
	}
	else
	{
		isdown = true;

		if( host_initialized )
			Host_WriteConfiguration();

		SV_ServerShutdown();
		Voice_Deinit();

		host_initialized = false;

		CDAudio_Shutdown();
		VGui_Shutdown();

		if( cls.state != ca_dedicated )
			ClientDLL_Shutdown();

		Cmd_RemoveGameCmds();
		Cmd_Shutdown();
		Cvar_Shutdown();

		HPAK_FlushHostQueue();
		SV_DeallocateDynamicData();

		for( int i = 0; i < svs.maxclientslimit; ++i )
		{
			SV_ClearFrames( &svs.clients[ i ].frames );
		}

		SV_Shutdown();
		SystemWrapper_ShutDown();

		NET_Shutdown();
		S_Shutdown();
		Con_Shutdown();

		ReleaseEntityDlls();

		CL_ShutDownClientStatic();
		CM_FreePAS();

		if( wadpath )
		{
			Mem_Free( wadpath );
			wadpath = nullptr;
		}

		if( cls.state != ca_dedicated )
			Draw_Shutdown();

		Draw_DecalShutdown();

		W_Shutdown();

		Log_Printf( "Server shutdown\n" );
		Log_Close();

		COM_Shutdown();
		CL_Shutdown();
		DELTA_Shutdown();

		Key_Shutdown();

		realtime = 0;

		//TODO: implement - Solokiller
		//sv.time = 0;
		cl.time = 0;
	}
}

/*
===============
Host_FilterTime

Computes simulation time (FPS value)
===============
*/
bool Host_FilterTime( float time )
{
	double fps;
	static int command_line_ticrate = -1;

	if (host_framerate.value > 0.0f)
	{
		if (Host_IsSinglePlayerGame() || cls.demoplayback)
		{
			host_frametime = sys_timescale.value * host_framerate.value;
			realtime += host_frametime;
			return true;
		}
	}

	realtime += sys_timescale.value * time;

	if (g_bIsDedicatedServer)
	{
		if (command_line_ticrate == -1)
			command_line_ticrate = COM_CheckParm("-sys_ticrate");

		if (command_line_ticrate > 0)
			fps = Q_atof(com_argv[command_line_ticrate + 1]);
		else
			fps = sys_ticrate.value;

		host_frametime = realtime - oldrealtime;

		if (fps > 0.0f)
		{
			if (1.0f / (fps + 1.0f) > host_frametime)
				return false;
		}
	}
	else
	{
		fps = fps_max.value;
		if (sv.active == false && cls.state != ca_disconnected && cls.state != ca_active)
			fps = 31.0;

		// Limit fps to withing tolerable range
		fps = __max(MIN_FPS, fps);
		if (!fps_override.value)
			fps = __min(MAX_FPS, fps);

		if (cl.maxclients > 1)
		{
			if (fps < 20.0f)
				fps = 20.0f;
		}

		if (gl_vsync.value)
		{
			if (!fps_override.value)
				fps = MAX_FPS;
		}

		host_frametime = realtime - oldrealtime;

		if (!cls.timedemo)
		{
			if (sys_timescale.value / (fps + 0.5f) > host_frametime)
				return false;
		}
	}

	oldrealtime = realtime;

	if (host_frametime > 0.25f)
		host_frametime = 0.25f;

	return true;
}

double rolling_fps = 0.0;

void Host_ComputeFPS( double frametime )
{
	rolling_fps = frametime * 0.4 + rolling_fps * 0.6;
}

/*
=====================
Host_UpdateScreen

Refresh the screen
=====================
*/
void Host_UpdateScreen( void )
{
	if (gfBackground)
		return;

	// Refresh the screen
	SCR_UpdateScreen();

	//if (cl_inmovie) TODO: Implement
	{
		//if (scr_con_current == 0.0)
			//VID_WriteBuffer(nullptr);
	}
}

void _Host_Frame( float time )
{
	static double host_times[6];

	if (setjmp(host_enddemo))
		return;			// demo finished.

	// decide the simulation time
	if (!Host_FilterTime(time))
		return;

	SystemWrapper_RunFrame(host_frametime);

	if (g_modfuncs.m_pfnFrameBegin)
		g_modfuncs.m_pfnFrameBegin();

	Host_ComputeFPS(host_frametime);

	//TODO: implement - Solokiller

	Cbuf_Execute();

	ClientDLL_UpdateClientData();

	//TODO: implement - Solokiller

	//ClientDLL_Frame(host_frametime); TODO: implement

	//TODO: implement - Solokiller

	Host_UpdateScreen();

	//TODO: implement - Solokiller

	//host_framecount++; TODO: Implement

	// TODO: Implement
}

int Host_Frame( float time, int iState, int* stateInfo )
{
	double time1, time2;

	if (setjmp(host_abortserver))
	{
		return giActive;
	}

	if (giActive != DLL_CLOSE || !g_iQuitCommandIssued)
		giActive = iState;

	*stateInfo = 0;

	if (host_profile.value)
		time1 = Sys_FloatTime();

	_Host_Frame(time);

	if (host_profile.value)
		time2 = Sys_FloatTime();

	if (giStateInfo)
	{
		*stateInfo = giStateInfo;
		giStateInfo = 0;
		Cbuf_Execute();
	}

	if (host_profile.value)
	{
		static double timetotal = 0;
		static int timecount = 0;

		timecount++;
		timetotal += time2 - time1;

		// Print status every 1000 frames
		if (timecount >= 1000)
		{
			int i;
			int iActiveClients = 0;

			for (i = 0; i < svs.maxclients; i++)
			{
				if (svs.clients[i].active)
					iActiveClients++;
			}

			Con_Printf("host_profile: %2i clients %2i msec\n", iActiveClients, (int)(floor(timetotal * 1000.0 / timecount)));

			timecount = 0;
			timetotal = 0;
		}
	}

	return giActive;
}

bool Host_IsServerActive()
{
	return sv.active;
}

bool Host_IsSinglePlayerGame()
{
	if( sv.active )
		return svs.maxclients == 1;
	else
		return cl.maxclients == 1;
}

void Host_GetHostInfo( float* fps, int* nActive, int* unused, int* nMaxPlayers, char* pszMap )
{
	int clients = 0;

	if( rolling_fps > 0.0 )
	{
		*fps = 1.0 / rolling_fps;
	}
	else
	{
		rolling_fps = 0.0;
		*fps = rolling_fps;
	}

	SV_CountPlayers( &clients );
	*nActive = clients;

	if( unused )
		*unused = 0;

	if( pszMap )
	{
		if( sv.name[ 0 ] )
			Q_strcpy( pszMap, sv.name );
		else
			*pszMap = '\0';
	}

	*nMaxPlayers = svs.maxclients;
}

void SV_DropClient( client_t* cl, qboolean crash, const char* fmt, ... )
{
	//TODO: implement - Solokiller
}

//TODO: typo - Solokiller
void Host_CheckDyanmicStructures()
{
	auto pClient = svs.clients;

	for( int i = 0; i < svs.maxclientslimit; ++i, ++pClient )
	{
		if( pClient->frames )
		{
			SV_ClearFrames( &pClient->frames );
		}
	}
}

void SV_ClearClientStates()
{
	auto pClient = svs.clients;

	for( int i = 0; i < svs.maxclients; ++i, ++pClient )
	{
		COM_ClearCustomizationList( &pClient->customdata, false );
		SV_ClearResourceLists( pClient );
	}
}

void Host_ClearMemory( bool bQuiet )
{
	CM_FreePAS();
	SV_ClearEntities();

	if( !bQuiet )
		Con_DPrintf( "Clearing memory\n" );

	D_FlushCaches();
	Mod_ClearAll();

	if( host_hunklevel )
	{
		Host_CheckDyanmicStructures();

		Hunk_FreeToLowMark( host_hunklevel );
	}

	cls.signon = 0;

	SV_ClearCaches();

	Q_memset( &sv, 0, sizeof( sv ) );

	CL_ClearClientState();

	SV_ClearClientStates();
}
