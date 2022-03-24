#include "quakedef.h"
#include "hashpak.h"

qboolean gp_hpak_queue;

void HPAK_FlushHostQueue()
{
	//TODO: implement - Solokiller
}

void HPAK_Init()
{
	/*
	Cmd_AddCommand("hpklist", HPAK_List_f);
	Cmd_AddCommand("hpkremove", HPAK_Remove_f);
	Cmd_AddCommand("hpkval", HPAK_Validate_f);
	Cmd_AddCommand("hpkextract", HPAK_Extract_f);
	gp_hpak_queue = false;
	*/
}

void HPAK_CheckIntegrity( const char* pakname )
{
	//TODO: implement - Solokiller
}

void HPAK_AddLump( bool bUseQueue, const char* pakname, resource_t* pResource, void* pData, FileHandle_t fpSource )
{
	//TODO: implement - Solokiller
}

int HPAK_ResourceForHash( const char* pakname, byte* hash, resource_t* pResourceEntry )
{
	//TODO: implement - Solokiller
	return 0;
}
