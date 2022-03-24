#include "quakedef.h"
#include "hashpak.h"

hash_pack_queue_t* gp_hpak_queue = NULL;

void HPAK_FlushHostQueue()
{
	for (hash_pack_queue_t* p = gp_hpak_queue; gp_hpak_queue != NULL; p = gp_hpak_queue)
	{
		gp_hpak_queue = p->next;
		HPAK_AddLump(0, p->pakname, &p->resource, p->data, 0);
		Mem_Free(p->pakname);
		Mem_Free(p->data);
		Mem_Free(p);
	}
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
