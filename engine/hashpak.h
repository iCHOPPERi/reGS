#ifndef ENGINE_HASHPAK_H
#define ENGINE_HASHPAK_H

#include "custom.h"

typedef struct hash_pack_queue_s
{
	char* pakname;
	resource_t resource;
	int datasize;
	void* data;
	hash_pack_queue_s* next;
} hash_pack_queue_t;

void HPAK_FlushHostQueue();

void HPAK_Init();

void HPAK_CheckIntegrity( const char* pakname );

void HPAK_AddLump( bool bUseQueue, const char* pakname, resource_t* pResource, void* pData, FileHandle_t fpSource );

int HPAK_ResourceForHash( const char* pakname, byte* hash, resource_t* pResourceEntry );

#endif //ENGINE_HASHPAK_H
