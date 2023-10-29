/***************************************************************************
 *   Copyright (C) 2007 Ryan Schultz, PCSX-df Team, PCSX team              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02111-1307 USA.           *
 ***************************************************************************/

/*
* Sound (SPU) functions.
*/

#include "spu.h"
#include "dfsound/externals.h"

char spuMemC[512 * 1024] __attribute__((aligned(32)));
char s_chan[(MAXCHAN + 1) * sizeof(spu.s_chan[0])] __attribute__((aligned(32)));
char rvb[sizeof(REVERBInfo)] __attribute__((aligned(32)));
char SB[MAXCHAN * sizeof(spu.SB[0]) * SB_SIZE] __attribute__((aligned(32)));

char pSpuBuffer[WII_SPU_FREQ] __attribute__((aligned(32)));
char SSumLR[NSSIZE * 2 * sizeof(spu.SSumLR[0])] __attribute__((aligned(32)));

char XABuf[WII_SPU_FREQ * sizeof(uint32_t) * 2] __attribute__((aligned(32)));
char CDDABuf[CDDA_BUFFER_SIZE] __attribute__((aligned(32)));


void CALLBACK SPUirq(int cycles_after) {
	if (cycles_after > 0) {
		set_event(PSXINT_SPU_IRQ, cycles_after);
		return;
	}

	psxHu32ref(0x1070) |= SWAPu32(0x200);
}

void spuDelayedIrq() {
	psxHu32ref(0x1070) |= SWAPu32(0x200);
}

// spuUpdate
void CALLBACK SPUschedule(unsigned int cycles_after) {
	set_event(PSXINT_SPU_UPDATE, cycles_after);
}

void spuUpdate() {
	SPU_async(psxRegs.cycle, 0, Config.PsxType);
}
