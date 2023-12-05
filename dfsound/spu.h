/***************************************************************************
                            spu.h  -  description
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

#ifndef __P_SPU_H__
#define __P_SPU_H__

#define HTOLE16(x) __builtin_bswap16(x)
#define LE16TOH(x) __builtin_bswap16(x)

void ClearWorkingState(void);
void CALLBACK DF_SPUplayADPCMchannel(xa_decode_t *xap, unsigned int cycle, int is_start);
int  CALLBACK DF_SPUplayCDDAchannel(short *pcm, int bytes, unsigned int cycle, int is_start);
void CALLBACK DF_SPUsetCDvol(unsigned char ll, unsigned char lr,
		unsigned char rl, unsigned char rr, unsigned int cycle);
void FeedXA(const xa_decode_t *xap);
void FeedCDDA(unsigned char *pcm, int nBytes);

#endif /* __P_SPU_H__ */
