/***************************************************************************
                          key.c  -  description
                             -------------------
    begin                : Sun Oct 28 2001
    copyright            : (C) 2001 by Pete Bernert
    email                : BlackDove@addcom.de
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
// 2005/04/15 - Pete
// - Changed user frame limit to floating point value
//
// 2002/10/04 - Pete
// - added Full vram view toggle key
//
// 2002/04/20 - linuzappz
// - added iFastFwd key
//
// 2002/02/23 - Pete
// - added toggle capcom fighter game fix
//
// 2001/12/22 - syo
// - added toggle wait VSYNC key handling
//   (Pete: added 'V' display in the fps menu)
//
// 2001/12/15 - lu
// - Yet another keyevent handling...
//
// 2001/11/09 - Darko Matesic
// - replaced info key with recording key (sorry Pete)
//   (Pete: added new recording key... sorry Darko ;)
//
// 2001/10/28 - Pete  
// - generic cleanup for the Peops release
//
//*************************************************************************// 

#include "stdafx.h"

#define _IN_KEY

#include "externals.h"
#include "menu.h"
#include "gpu.h"
#include "draw.h"
#include "key.h"

////////////////////////////////////////////////////////////////////////

void GPUmakeSnapshot(void);

unsigned long          ulKeybits=0;

void GPUkeypressed(int keycode)
{
}

////////////////////////////////////////////////////////////////////////

void SetKeyHandler(void)
{
}

////////////////////////////////////////////////////////////////////////

void ReleaseKeyHandler(void)
{
}
