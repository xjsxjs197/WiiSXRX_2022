/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version. See also the license.txt file for *
 *   additional informations.                                              *
 *                                                                         *
 ***************************************************************************/
#include "stdafx.h"
#include "externals.h"

short g_m1=255,g_m2=255,g_m3=255;
short DrawSemiTrans=FALSE;
short Ymin;
short Ymax;
int  gInterlaceLine=1;

short          ly0,lx0,ly1,lx1,ly2,lx2,ly3,lx3;        // global psx vertex coords
long           GlobalTextAddrX,GlobalTextAddrY,GlobalTextTP;
long           GlobalTextABR,GlobalTextPAGE;

BOOL           bUsingTWin=FALSE;
TWin_t         TWin;
unsigned short usMirror=0;                             // sprite mirror
int            iDither=0;
long        drawX;
long        drawY;
long        drawW;
long        drawH;
unsigned int dwCfgFixes;
unsigned int dwActFixes=0;
int            iUseFixes;
int            iUseDither=0;
BOOL           bDoVSyncUpdate=FALSE;
int            iFakePrimBusy = 0;

int				  gMouse[4];

unsigned char dithertable[16] =
{
    7, 0, 6, 1,
    2, 5, 3, 4,
    1, 6, 0, 7,
    4, 3, 5, 2
};

///////////////////////////////////////////////////////////////////////
