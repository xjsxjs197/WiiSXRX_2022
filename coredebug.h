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
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.           *
 ***************************************************************************/

/*
* Specficies which logs should be activated.
* Ryan TODO: These should ALL be definable with configure flags.
*/

#ifndef __CORE_DEBUG_H__
#define __CORE_DEBUG_H__

#include <gctypes.h>
#include <stdint.h>

extern char *disRNameCP0[];

char* disR3000AF(u32 code, u32 pc);

#if defined (CPU_LOG) || defined(DMA_LOG) || defined(CDR_LOG) || defined(HW_LOG) || \
	defined(PSXBIOS_LOG) || defined(GTE_LOG) || defined(PAD_LOG)
extern FILE *emuLog;
#endif

//#define GTE_DUMP

#ifdef GTE_DUMP
FILE *gteLog;
#endif

//#define LOG_STDOUT

//#define PAD_LOG  __Log
//#define GTE_LOG  __Log
//#define CDR_LOG  __Log("%8.8lx %8.8lx: ", psxRegs.pc, psxRegs.cycle); __Log

//#define PSXHW_LOG   __Log("%8.8lx %8.8lx: ", psxRegs.pc, psxRegs.cycle); __Log
//#define PSXBIOS_LOG __Log("%8.8lx %8.8lx: ", psxRegs.pc, psxRegs.cycle); __Log
//#define PSXDMA_LOG  __Log
//#define PSXMEM_LOG  __Log("%8.8lx %8.8lx: ", psxRegs.pc, psxRegs.cycle); __Log
//#define PSXCPU_LOG  __Log

//#define CDRCMD_DEBUG

#if defined (PSXCPU_LOG) || defined(PSXDMA_LOG) || defined(CDR_LOG) || defined(PSXHW_LOG) || \
	defined(PSXBIOS_LOG) || defined(PSXMEM_LOG) || defined(GTE_LOG)    || defined(PAD_LOG)
#define EMU_LOG __Log
#endif

// add xjsxjs197 start
#ifdef DISP_DEBUG
    //extern void PEOPS_GPUdisplayText(char * pText);
    extern char debug[256];

    #define PRINT_LOG(msg) { \
                sprintf(debug, msg);\
                PEOPS_GPUdisplayText(debug); \
            }
    #define PRINT_LOG1(msg, val) { \
                sprintf(debug, msg, val);\
                PEOPS_GPUdisplayText(debug); \
            }
    #define PRINT_LOG2(msg, val1, val2) { \
                sprintf(debug, msg, val1, val2);\
                PEOPS_GPUdisplayText(debug); \
            }
    #define PRINT_LOG3(msg, val1, val2, val3) { \
                sprintf(debug, msg, val1, val2, val3);\
                PEOPS_GPUdisplayText(debug); \
            }
#else
    #define PRINT_LOG(msg)
    #define PRINT_LOG1(msg, val)
    #define PRINT_LOG2(msg, val1, val2)
    #define PRINT_LOG3(msg, val1, val2, val3)
#endif
// add xjsxjs197 end

#endif /* __DEBUG_H__ */
