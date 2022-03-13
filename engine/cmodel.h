#ifndef ENGINE_MODEL_H
#define ENGINE_MODEL_H

extern unsigned char* gPAS;
extern unsigned char* gPVS;
const int MODEL_MAX_PVS = 1024;

void Mod_Init();

void CM_FreePAS();

#endif //ENGINE_MODEL_H
