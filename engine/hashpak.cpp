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

void HPAK_AddLump(bool bUseQueue, const char* pakname, resource_t* pResource, void* pData, FileHandle_t fpSource)
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

void HPAK_CheckIntegrity(char* pakname)
{
	char name[256];
	Q_snprintf(name, sizeof(name), "%s", pakname);

	COM_DefaultExtension(name, HASHPAK_EXTENSION);
	COM_FixSlashes(name);
	// HPAK_ValidatePak(name); - TODO: implement - ScriptedSnark
}

qboolean HPAK_FindResource(hash_pack_directory_t* pDir, unsigned char* hash, struct resource_s* pResourceEntry)
{
	for (int i = 0; i < pDir->nEntries; i++)
	{
		if (Q_memcmp(hash, pDir->p_rgEntries[i].resource.rgucMD5_hash, 16) == 0)
		{
			if (pResourceEntry)
				Q_memcpy(pResourceEntry, &pDir->p_rgEntries[i].resource, sizeof(resource_t));

			return TRUE;
		}
	}
	return FALSE;
}

qboolean HPAK_ResourceForHash(char* pakname, unsigned char* hash, struct resource_s* pResourceEntry)
{
	qboolean bFound;
	hash_pack_header_t header;
	hash_pack_directory_t directory;
	char name[MAX_PATH];
	FileHandle_t fp;

	if (gp_hpak_queue)
	{
		for (hash_pack_queue_t* p = gp_hpak_queue; p != NULL; p = p->next)
		{
			if (Q_stricmp(p->pakname, pakname) != 0 || Q_memcmp(p->resource.rgucMD5_hash, hash, 16) != 0)
				continue;

			if (pResourceEntry)
				Q_memcpy(pResourceEntry, &p->resource, sizeof(resource_t));

			return TRUE;
		}
	}

	Q_snprintf(name, ARRAYSIZE(name), "%s", pakname);

	COM_DefaultExtension(name, HASHPAK_EXTENSION);
	fp = FS_Open(name, "rb");

	if (!fp)
	{
		Con_Printf("ERROR: couldn't open %s.\n", name);
		return FALSE;
	}

	FS_Read(&header, sizeof(hash_pack_header_t), fp);

	if (Q_strncmp(header.szFileStamp, "HPAK", sizeof(header.szFileStamp)))
	{
		Con_Printf("%s is not an HPAK file\n", name);
		FS_Close(fp);
		return FALSE;
	}

	if (header.version != HASHPAK_VERSION)
	{
		Con_Printf("HPAK_List:  version mismatch\n");
		FS_Close(fp);
		return FALSE;
	}

	FS_Seek(fp, header.nDirectoryOffset, FILESYSTEM_SEEK_HEAD);
	FS_Read(&directory.nEntries, 4, fp);

	if (directory.nEntries < 1 || (unsigned int)directory.nEntries > MAX_FILE_ENTRIES)
	{
		Con_Printf("ERROR: HPAK had bogus # of directory entries:  %i\n", directory.nEntries);
		FS_Close(fp);
		return FALSE;
	}

	directory.p_rgEntries = (hash_pack_entry_t*)Mem_Malloc(sizeof(hash_pack_entry_t) * directory.nEntries);
	FS_Read(directory.p_rgEntries, sizeof(hash_pack_entry_t) * directory.nEntries, fp);

	bFound = HPAK_FindResource(&directory, hash, pResourceEntry);

	FS_Close(fp);
	Mem_Free(directory.p_rgEntries);

	return bFound;
}
