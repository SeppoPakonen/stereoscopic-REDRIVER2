#ifndef SKY_H
#define SKY_H

#include "engine/tset.h"   // struct UV

extern unsigned char HorizonTextures[40];
extern struct UV skytexuv[28];
extern short skyclut[28];
extern short skytpage[28];

extern SVECTOR sun_position[4];
extern SVECTOR moon_position[4];

extern int gTunnelNum;

extern void LoadSky(); // 0x000775C8

extern void DrawSkyDome(); // 0x0007795C
extern void DrawLensFlare(); // 0x00077A8C

extern void TunnelSkyFade(); // 0x000787B0

#endif
