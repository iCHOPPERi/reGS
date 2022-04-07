#include "quakedef.h"
#include "cl_ents.h"
#include <cl_entity.h>

void CL_SetSolidPlayers( int playernum )
{
	//TODO: implement - Solokiller
}

void CL_SetUpPlayerPrediction( bool dopred, bool bIncludeLocalClient )
{
	//TODO: implement - Solokiller
}

void CL_AddStateToEntlist( physent_t* pe, entity_state_t* state )
{
	//TODO: implement - Solokiller
}

qboolean CL_EntityTeleported(cl_entity_t* ent)
{
	if (ent->curstate.origin[0] - ent->prevstate.origin[0] <= 128.0	&& ent->curstate.origin[1] - ent->prevstate.origin[1] <= 128.0)
	{
		return ent->curstate.origin[2] - ent->prevstate.origin[2] > 128.0;
	}

	return true;
}
