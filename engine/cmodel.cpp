#include "quakedef.h"
#include "cmodel.h"
#include "gl_model.h"

unsigned char* gPAS;
unsigned char* gPVS;
int gPVSRowBytes;
unsigned char mod_novis[MODEL_MAX_PVS];

void Mod_Init()
{
	Q_memset(mod_novis, 255, MODEL_MAX_PVS);
}

unsigned char* CM_LeafPVS(int leafnum)
{
	if (gPVS)
	{
		return &gPVS[gPVSRowBytes * leafnum];
	}

	return mod_novis;
}

unsigned char* CM_LeafPAS(int leafnum)
{
	if (gPAS)
	{
		return &gPAS[gPVSRowBytes * leafnum];
	}

	return mod_novis;
}

unsigned char* Mod_LeafPVS(mleaf_t* leaf, model_t* model)
{
	if (leaf == model->leafs)
	{
		return mod_novis;
	}

	if (!gPVS)
	{
		return Mod_DecompressVis(leaf->compressed_vis, model);
	}

	int leafnum = leaf - model->leafs;
	return CM_LeafPVS(leafnum);
}

void CM_FreePAS()
{
	if (gPAS)
		Mem_Free(gPAS);
	if (gPVS)
		Mem_Free(gPVS);
	gPAS = 0;
	gPVS = 0;
}
