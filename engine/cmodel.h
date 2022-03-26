#ifndef ENGINE_MODEL_H
#define ENGINE_MODEL_H

extern unsigned char* gPAS;
extern unsigned char* gPVS;
const int MODEL_MAX_PVS = 1024;

void Mod_Init();
unsigned char* CM_LeafPVS(int leafnum);
unsigned char* CM_LeafPAS(int leafnum);
void CM_FreePAS();

#endif //ENGINE_MODEL_H
