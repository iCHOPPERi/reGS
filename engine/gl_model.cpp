/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
/**
*	@file
*
*	model loading and caching
*
*	models are the only shared resource between a client and server running
*	on the same machine.
*/
#include "quakedef.h"
#include "qgl.h"
#include "gl_mesh.h"
#include "gl_model.h"
#include <host.h>
#include <client.h>

model_t* loadmodel;
char loadname[32];

struct mod_known_info_t
{
	qboolean		shouldCRC;		// Needs a CRC check?
	qboolean		firstCRCDone;	// True when the first CRC check has been finished

	CRC32_t			initialCRC;
};

mod_known_info_t mod_known_info[MAX_MOD_KNOWN];

byte* mod_base = nullptr;

/*
==============================================================================

ALIAS MODELS

==============================================================================
*/

aliashdr_t	*pheader;

stvert_t	stverts[ MAXALIASVERTS ];
mtriangle_t	triangles[ MAXALIASTRIS ];

// a pose is a single set of vertexes.  a frame may be
// an animating sequence of poses
trivertx_t	*poseverts[ MAXALIASFRAMES ];
int			posenum;

char* wadpath = nullptr;
int	mod_numknown;
model_t	mod_known[1024];

void* Mod_Extradata(model_t* mod)
{
	void* result;

	if (!mod)
		return nullptr;

	result = Cache_Check(&mod->cache);

	if (result)
		return result;

	Mod_LoadModel(mod, true, false);

	if (!mod->cache.data)
		Sys_Error("Mod_Extradata: caching failed");

	return mod->cache.data;
}

void Mod_ClearAll()
{
	int i;
	model_t* mod;

	for (i = 0, mod = mod_known; i < mod_numknown; i++, mod++)
	{
		if (mod->type != mod_alias && mod->needload != (NL_NEEDS_LOADED | NL_UNREFERENCED))
		{
			mod->needload = NL_UNREFERENCED;

			if (mod->type == mod_sprite)
				mod->cache.data = nullptr;
		}
	}
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

void Mod_FillInCRCInfo(bool trackCRC, int model_number)
{
	mod_known_info[model_number].firstCRCDone = false;
	mod_known_info[model_number].shouldCRC = trackCRC;
	mod_known_info[model_number].initialCRC = 0;
}

model_t* Mod_FindName(bool trackCRC, const char* name)
{
	int	i;
	model_t* mod, *avail = nullptr;

	if (!name[0])
		Sys_Error("Mod_FindName: NULL name");

	for (i = 0, mod = mod_known; i < mod_numknown; i++, mod++)
	{
		if (!Q_stricmp(mod->name, name))
			break;

		if (mod->needload == NL_UNREFERENCED)
		{
			if (!avail || mod->type != mod_alias && mod->type != mod_studio)
				avail = mod;
		}
	}

	if (i == mod_numknown)
	{
		if (mod_numknown < MAX_MOD_KNOWN)
		{
			Mod_FillInCRCInfo(trackCRC, mod_numknown);
			mod_numknown++;
		}
		else
		{
			if (!avail)
				Sys_Error("Mod_FindName: mod_numknown >= MAX_KNOWN_MODELS");

			mod = avail;
			Mod_FillInCRCInfo(trackCRC, avail - mod_known);
		}

		Q_strncpy(mod->name, name, sizeof(mod->name) - 1);
		mod->name[sizeof(mod->name) - 1] = 0;

		if (mod->needload != (NL_NEEDS_LOADED | NL_UNREFERENCED))
			mod->needload = NL_NEEDS_LOADED;
	}

	return mod;
}

void Mod_ChangeGame(void)
{
	int i;
	model_t* mod = nullptr;
	mod_known_info_t* p = nullptr;

	for (i = 0; i < mod_numknown; i++)
	{
		mod = &mod_known[i];

		if (mod->type == mod_studio)
		{
			if (Cache_Check(&mod->cache))
				Cache_Free(&mod->cache);
		}

		p = &mod_known_info[i];
		p->firstCRCDone = false;
		p->initialCRC = 0;
	}
}

model_t* Mod_LoadModel(model_t* mod, const bool crash, const bool trackCRC)
{
	CRC32_t currentCRC;
	char tmpName[MAX_PATH];

	FileHandle_t file;

	byte* buf = nullptr;
	mod_known_info_t* pinfo = nullptr;

	if (mod->type == mod_alias || mod->type == mod_studio)
	{
		if (Cache_Check(&mod->cache))
		{
			mod->needload = NL_PRESENT;
			return mod;
		}
	}
	else
	{
		if (mod->needload == NL_PRESENT || mod->needload == (NL_NEEDS_LOADED | NL_UNREFERENCED))
			return mod;
	}

	// Check for -steam launch param
	if (COM_CheckParm("-steam") && mod->name[0] == '/')
	{
		char* p = mod->name;
		while (*(++p) == '/')
			;

		strncpy(tmpName, p, sizeof(tmpName) - 1);
		tmpName[sizeof(tmpName) - 1] = 0;

		strncpy(mod->name, tmpName, sizeof(mod->name) - 1);
		mod->name[sizeof(mod->name) - 1] = 0;
	}

	//
	// Load the file, check if it exists
	//
	file = FS_Open(mod->name, "rb");

	if (!file)
	{
		if (crash || developer.value > 1.0)
			Sys_Error("Mod_NumForName: %s not found", mod->name);

		Con_Printf("Error: could not load file %s\n", mod->name);
		return nullptr;
	}

	int length;
	buf = (byte*)FS_GetReadBuffer(file, &length);

	if (buf)
	{
		if (LittleLong(*(uint32*)buf) == 'TSDI')
		{
			FS_ReleaseReadBuffer(file, buf);
			buf = nullptr;
		}
	}

	if (!buf)
	{
		length = FS_Size(file);
		buf = (byte*)Hunk_TempAlloc(length + 1);
		FS_Read(buf, length, file);
		FS_Close(file);

		if (!buf)
		{
			if (crash || developer.value > 1.0)
				Sys_Error("Mod_NumForName: %s not found", mod->name);

			Con_Printf("Error: could not load file %s\n", mod->name);
			return nullptr;
		}

		file = 0;
	}

	if (trackCRC)
	{
		pinfo = &mod_known_info[mod - mod_known];

		if (pinfo->shouldCRC)
		{
			CRC32_Init(&currentCRC);
			CRC32_ProcessBuffer(&currentCRC, buf, length);
			currentCRC = CRC32_Final(currentCRC);

			if (pinfo->firstCRCDone)
			{
				if (currentCRC != pinfo->initialCRC)
					Sys_Error("%s has been modified since starting the engine.  Consider running system diagnostics to check for faulty hardware.\n", mod->name);
			}
			else
			{
				pinfo->firstCRCDone = true;
				pinfo->initialCRC = currentCRC;

				SetCStrikeFlags();

				if (!IsGameSubscribed("czero") && g_bIsCStrike /* && IsCZPlayerModel(currentCRC, mod->name)*/ && cls.state != ca_dedicated)
				{
					COM_ExplainDisconnection(true, "Cannot continue with altered model %s, disconnecting.", mod->name);
					// CL_Disconnect();
					return nullptr;
				}
			}
		}
	}

	if (developer.value > 1.0)
		Con_DPrintf("loading %s\n", mod->name);

	//
	// allocate a new model
	//
	COM_FileBase(mod->name, loadname);

	loadmodel = mod;

	//
	// fill it in
	//

	// call the apropriate loader
	mod->needload = NL_PRESENT;

	/* - TODO: implement - ScriptedSnark
	switch (LittleLong(*(unsigned*)buf))
	{
	case IDPOLYHEADER:
		Mod_LoadAliasModel(mod, buf);
		break;

	case IDSPRITEHEADER:
		Mod_LoadSpriteModel(mod, buf);
		break;

	case IDSTUDIOHEADER:
		Mod_LoadStudioModel(mod, buf);
		break;

	default:
		DT_LoadDetailMapFile(loadname);
		Mod_LoadBrushModel(mod, buf);
		break;
	}
	*/
	if (file)
	{
		FS_ReleaseReadBuffer(file, buf);
		FS_Close(file);
	}

	return mod;
}

void Mod_LoadEntities(lump_t* l)
{
	if (!l->filelen)
	{
		loadmodel->entities = nullptr;
		return;
	}

	loadmodel->entities = (char*)Hunk_AllocName(l->filelen, loadname);
	Q_memcpy(loadmodel->entities, mod_base + l->fileofs, l->filelen);

	if (loadmodel->entities)
	{
		char* pszInputStream = COM_Parse(loadmodel->entities);

		if (*pszInputStream)
		{
			while (com_token[0] != '}')
			{
				if (!Q_strcmp(com_token, "wad"))
				{
					COM_Parse(pszInputStream);

					if (wadpath)
						Mem_Free(wadpath);

					wadpath = Mem_Strdup(com_token);
					return;
				}

				pszInputStream = COM_Parse(pszInputStream);

				if (!*pszInputStream)
					return;
			}
		}
	}
}

void Mod_LoadLighting(lump_t* l)
{
	if (!l->filelen)
	{
		loadmodel->lightdata = nullptr;
		return;
	}

	loadmodel->lightdata = (color24*)Hunk_AllocName(l->filelen, loadname);
	Q_memcpy(loadmodel->lightdata, mod_base + l->fileofs, l->filelen);
}

void Mod_LoadVisibility(lump_t* l)
{
	if (!l->filelen)
	{
		loadmodel->visdata = nullptr;
		return;
	}

	loadmodel->visdata = (byte*)Hunk_AllocName(l->filelen, loadname);
	Q_memcpy(loadmodel->visdata, mod_base + l->fileofs, l->filelen);
}

void Mod_LoadVertexes(lump_t* l)
{
	dvertex_t* in;
	mvertex_t* out;
	int	i, count;

	in = (dvertex_t*)(mod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
		Sys_Error("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = (mvertex_t*)Hunk_AllocName(count * sizeof(*out), loadname);

	loadmodel->vertexes = out;
	loadmodel->numvertexes = count;

	for (i = 0; i < count; i++, in++, out++)
	{
		out->position[0] = LittleFloat(in->point[0]);
		out->position[1] = LittleFloat(in->point[1]);
		out->position[2] = LittleFloat(in->point[2]);
	}
}

void Mod_LoadSubmodels(lump_t* l)
{
	dmodel_t* in;
	dmodel_t* out;
	int	i, j, count;
	in = (dmodel_t*)(mod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
		Sys_Error("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = (dmodel_t*)Hunk_AllocName(count * sizeof(*out), loadname);

	loadmodel->submodels = out;
	loadmodel->numsubmodels = count;

	for (i = 0; i < count; i++, in++, out++)
	{
		for (j = 0; j < 3; j++)
		{
			out->mins[j] = LittleFloat(in->mins[j]) - 1;
			out->maxs[j] = LittleFloat(in->maxs[j]) + 1;
			out->origin[j] = LittleFloat(in->origin[j]);
		}

		for (j = 0; j < MAX_MAP_HULLS; j++)
			out->headnode[j] = LittleLong(in->headnode[j]);

		out->visleafs = LittleLong(in->visleafs);
		out->firstface = LittleLong(in->firstface);
		out->numfaces = LittleLong(in->numfaces);
	}
}

void Mod_LoadEdges(lump_t* l)
{
	dedge_t* in;
	medge_t* out;
	int i, count;

	in = (dedge_t*)(mod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
		Sys_Error("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = (medge_t*)Hunk_AllocName((count + 1) * sizeof(*out), loadname);

	loadmodel->edges = out;
	loadmodel->numedges = count;

	for (i = 0; i < count; i++, in++, out++)
	{
		out->v[0] = (unsigned short)LittleShort(in->v[0]);
		out->v[1] = (unsigned short)LittleShort(in->v[1]);
	}
}

void Mod_LoadSurfedges(lump_t* l)
{
	int	i, count;
	int* in, *out;
	in = (int*)(mod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
		Sys_Error("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = (int*)Hunk_AllocName(count * sizeof(*out), loadname);

	loadmodel->surfedges = out;
	loadmodel->numsurfedges = count;

	for (i = 0; i < count; i++)
		out[i] = LittleLong(in[i]);
}

void Mod_LoadPlanes(lump_t* l)
{
	int			i, j;
	mplane_t* out;
	dplane_t* in;
	int			count;
	int			bits;

	in = (dplane_t*)(mod_base + l->fileofs);

	if (l->filelen % sizeof(*in))
		Sys_Error("MOD_LoadBmodel: funny lump size in %s", loadmodel->name);

	count = l->filelen / sizeof(*in);
	out = (mplane_t*)Hunk_AllocName(count * 2 * sizeof(*out), loadname);

	loadmodel->planes = out;
	loadmodel->numplanes = count;

	for (i = 0; i < count; i++, in++, out++)
	{
		bits = 0;

		for (j = 0; j < 3; j++)
		{
			out->normal[j] = LittleFloat(in->normal[j]);

			if (out->normal[j] < 0)
				bits |= 1 << j;
		}

		out->dist = LittleFloat(in->dist);
		out->type = LittleLong(in->type);
		out->signbits = bits;
	}
}

float RadiusFromBounds(vec_t* mins, vec_t* maxs)
{
	int		i;
	vec3_t	corner;

	for (i = 0; i < 3; i++)
	{
		corner[i] = fabs(mins[i]) > fabs(maxs[i]) ? fabs(mins[i]) : fabs(maxs[i]);
	}

	return Length(corner);
}

void Mod_UnloadSpriteTextures(model_t* pModel)
{
	void* pvVar1;
	int param4;
	int iVar2;
	char name[256];

	if (pModel->type == mod_sprite) {
		pvVar1 = (pModel->cache).data;
		pModel->needload = true;
		if ((pvVar1 != (void*)0x0) && (0 < *(int*)((int)pvVar1 + 0xc))) {
			param4 = 0;
			do {
				iVar2 = param4 + 1;
				snprintf(name, 0x100, "%s_%i", pModel->name, param4);
				// TODO: impl - xWhitey
				//GL_UnloadTexture(name);
				param4 = iVar2;
			} while (*(int*)((int)pvVar1 + 0xc) != iVar2 && iVar2 <= *(int*)((int)pvVar1 + 0xc));
		}
	}
	return;
}

mleaf_t* Mod_PointInLeaf(vec_t* p, model_t* model)
{
	mnode_t* node;
	float d;
	mplane_t* plane;

	if (!model || !model->nodes)
		Sys_Error("Mod_PointInLeaf: bad model");

	node = model->nodes;

	while (true)
	{
		if (node->contents < 0)
			return (mleaf_t*)node;

		plane = node->plane;

		d = DotProduct(p, plane->normal) - plane->dist;

		if (d > 0)
			node = node->children[0];
		else
			node = node->children[1];
	}

	return nullptr;	// never reached
}