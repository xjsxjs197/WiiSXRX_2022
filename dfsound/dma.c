/***************************************************************************
                            dma.c  -  description
                             -------------------
    begin                : Wed May 15 2002
    copyright            : (C) 2002 by Pete Bernert
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

#include "stdafx.h"

#define _IN_DMA

#include "externals.h"

////////////////////////////////////////////////////////////////////////
// READ DMA (one value)
////////////////////////////////////////////////////////////////////////

unsigned short DF_SPUreadDMA(void)
{
 unsigned short s = *(unsigned short *)(spu.spuMemC + spu.spuAddr);
 spu.spuAddr += 2;
 spu.spuAddr &= 0x7fffe;

 return s;
}

////////////////////////////////////////////////////////////////////////
// READ DMA (many values)
////////////////////////////////////////////////////////////////////////
#define DBG_SPU1	7
#define DBG_SPU2	8
#define DBG_SPU3	9

#ifdef SHOW_DEBUG
extern char txtbuffer[1024];
#endif // DISP_DEBUG

void DF_SPUreadDMAMem(unsigned short *pusPSXMem, int iSize,
 unsigned int cycles)
{
 int i;
 unsigned short crc=0;

 #ifdef SHOW_DEBUG
 sprintf(txtbuffer, "SPUreadDMA spuAddr %08x size %x crc %x", spu.spuAddr, iSize, crc);
 DEBUG_print(txtbuffer, DBG_SPU1);
 #endif // DISP_DEBUG

 for(i=0;i<iSize;i++)
  {
   *pusPSXMem = *(unsigned short *)(spu.spuMemC + spu.spuAddr);
   crc |= *pusPSXMem;
   pusPSXMem++;
   spu.spuAddr += 2;
   //spu.spuAddr &= 0x7fffe;
   // guess based on Vib Ribbon (below)
   if (spu.spuAddr > 0x7ffff) break;
  }
 /*
 /* Toshiden Subaru "story screen" hack.
 /*
 /* After character selection screen, the game checks values inside returned
 /* SPU buffer and all values cannot be 0x0.
 /* Due to XA timings(?) we return buffer that has only NULLs.
 /* Setting little lag to MixXA() causes buffer to have some non-NULL values,
 /* but causes garbage sound so this hack is preferable.
 /*
 /* Note: When messing with xa.c like fixing Suikoden II's demo video sound issue
 /* this should be handled as well.
 */
 //do_samples_if_needed(cycles, 1);

 #ifdef SHOW_DEBUG
 sprintf(txtbuffer, "SPUreadDMA crc %x", crc);
 DEBUG_print(txtbuffer, DBG_SPU3);
 #endif // DISP_DEBUG
 if (crc == 0) *--pusPSXMem=0xFF;
}

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////

// to investigate: do sound data updates by writedma affect spu
// irqs? Will an irq be triggered, if new data is written to
// the memory irq address?

////////////////////////////////////////////////////////////////////////
// WRITE DMA (one value)
////////////////////////////////////////////////////////////////////////

void DF_SPUwriteDMA(unsigned short val)
{
 *(unsigned short *)(spu.spuMemC + spu.spuAddr) = val;

 spu.spuAddr += 2;
 spu.spuAddr &= 0x7fffe;
 spu.bMemDirty = 1;
}

////////////////////////////////////////////////////////////////////////
// WRITE DMA (many values)
////////////////////////////////////////////////////////////////////////

void DF_SPUwriteDMAMem(unsigned short *pusPSXMem, int iSize,
 unsigned int cycles)
{
 int i;

 //do_samples_if_needed(cycles, 1);
 spu.bMemDirty = 1;

 if(spu.spuAddr + iSize*2 < 0x80000)
  {
   memcpy(spu.spuMemC + spu.spuAddr, pusPSXMem, iSize*2);
   spu.spuAddr += iSize*2;
   return;
  }

 for(i=0;i<iSize;i++)
  {
   *(unsigned short *)(spu.spuMemC + spu.spuAddr) = *pusPSXMem++;
   spu.spuAddr += 2;
   //spu.spuAddr &= 0x7fffe;
   // Vib Ribbon - stop transfer (reverb playback)
   if (spu.spuAddr > 0x7ffff) break;
  }
}

////////////////////////////////////////////////////////////////////////
