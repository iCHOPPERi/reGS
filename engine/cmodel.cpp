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

byte* Mod_DecompressVis(byte* in, model_t* model)
{
	// #define MAX_MAP_LEAFS 32767
	// i just copypasted this thing from quake - xWhitey
	static byte	decompressed[32767 / 8];
	int		c;
	byte* out;
	int		row;

	row = (model->numleafs + 7) >> 3;
	out = decompressed;

#if 0
	memcpy(out, in, row);
#else
	if (!in)
	{	// no vis info, so make all visible
		while (row)
		{
			*out++ = 0xff;
			row--;
		}
		return decompressed;
	}

	do
	{
		if (*in)
		{
			*out++ = *in++;
			continue;
		}

		c = in[1];
		in += 2;
		while (c)
		{
			*out++ = 0;
			c--;
		}
	} while (out - decompressed < row);
#endif

	return decompressed;
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
