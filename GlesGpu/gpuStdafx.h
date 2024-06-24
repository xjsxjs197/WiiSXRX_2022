/***************************************************************************
                          stdafx.h  -  description
                             -------------------
    begin                : Sun Mar 08 2009
    copyright            : (C) 1999-2009 by Pete Bernert
    web                  : www.pbernert.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version. See also the license.txt file for *
 *   additional informations.                                              *
 *                                                                         *
 ***************************************************************************/

//*************************************************************************//
// History of changes:
//
// 2009/03/08 - Pete
// - generic cleanup for the Peops release
//
//*************************************************************************//

#ifndef __GPU_STDAFX__
#define __GPU_STDAFX__

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _GPU_API_
#define _GPU_API_ 1
#endif


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <math.h>
#define SHADETEXBIT(x) ((x>>24) & 0x1)
#define SEMITRANSBIT(x) ((x>>25) & 0x1)

#define glError()

#define TEXTURE_MOVIE_IDX    48
#define TEXTURE_FRAME_IDX    49
#define TEXTURE_MAX_COUNT    50

extern int gxTextureIdx;
extern int gxTextureInfo[TEXTURE_MAX_COUNT][2]; // GxTexture width height info

#ifdef __cplusplus
}
#endif

#endif
