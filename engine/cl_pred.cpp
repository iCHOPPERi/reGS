#include "quakedef.h"
#include "cl_pred.h"
#include "pmove.h"

int pushed, oldphysent, oldvisent;

void CL_PushPMStates()
{
    if (pushed)
    {
        Con_Printf("CL_PushPMStates called with pushed stack\n");
    }
    else
    {
        oldphysent = pmove->numphysent;
        oldvisent = pmove->numvisent;
        pushed = 1;
    }
}

void CL_PopPMStates()
{
	//TODO: implement - Solokiller
}

void CL_RunUsercmd( local_state_t* from, local_state_t* to, usercmd_t* u, bool runfuncs, double* pfElapsed, unsigned int random_seed )
{
	//TODO: implement - Solokiller
}
