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

/*  Playstation Memory Map (from Playstation doc by Joshua Walker)
0x0000_0000-0x0000_ffff		Kernel (64K)
0x0001_0000-0x001f_ffff		User Memory (1.9 Meg)

0x1f00_0000-0x1f00_ffff		Parallel Port (64K)

0x1f80_0000-0x1f80_03ff		Scratch Pad (1024 bytes)

0x1f80_1000-0x1f80_2fff		Hardware Registers (8K)

0x8000_0000-0x801f_ffff		Kernel and User Memory Mirror (2 Meg) Cached

0xa000_0000-0xa01f_ffff		Kernel and User Memory Mirror (2 Meg) Uncached

0xbfc0_0000-0xbfc7_ffff		BIOS (512K)
*/

/*
* PSX memory functions.
*/

/* Ryan TODO: I'd rather not use GLib in here */

#include <malloc.h>
#include <gccore.h>
#include <stdlib.h>
#include "psxmem.h"
#include "r3000a.h"
#include "psxhw.h"
#include "Gamecube/fileBrowser/fileBrowser.h"
#include "Gamecube/fileBrowser/fileBrowser-libfat.h"
#include "Gamecube/fileBrowser/fileBrowser-CARD.h"
#include "Gamecube/fileBrowser/fileBrowser-DVD.h"
#include "Gamecube/wiiSXconfig.h"
#include "Gamecube/DEBUG.h"

#include "Gamecube/vm/vm.h"

bool lightrec_mmap_inited = false;

#include <ogc/machine/processor.h>
#include <ogc/cast.h>
#include <ogc/cache.h>

void CAST_SetGQR(s32 GQR, u32 typeL, s32 scaleL)
{
	//register u32 val = ((((scaleL)<<8)) | (typeL));
	//register u32 val = (((((scaleL)<<24)|(typeL))<<16));
	register u32 val = (((scaleL) << 24) | ((typeL) << 16) | ((scaleL) << 8) | (typeL));
	__set_gqr(GQR,val);
}

extern void SysMessage(char *fmt, ...);

static s8 psxM_buf[0x220000] __attribute__((aligned(4096)));
static s8 psxR_buf[0x80000] __attribute__((aligned(4096)));

s8 *psxM = psxM_buf; // Kernel & User Memory (2 Meg)
s8 *psxR = psxR_buf; // BIOS ROM (512K)
s8 *psxP = NULL; // Parallel Port (64K)
s8 *psxH = NULL; // Scratch Pad (1K) & Hardware Registers (8K)

//s8 *psxH = NULL; // Scratch Pad (1K) & Hardware Registers (8K)

//s8 psxM[0x00220000] __attribute__((aligned(32)));
//s8 psxR[0x00080000] __attribute__((aligned(32)));
u8* psxMemWLUT[0x10000] __attribute__((aligned(32)));
u8* psxMemRLUT[0x10000] __attribute__((aligned(32)));

#define BUF_SIZE 0x400000 // 4 MiB code buffer for Lightrec and DYNAREC
extern char recBuffer[BUF_SIZE] __attribute__((aligned(32)));

int psxMemInit() {
	int i;

	psxP = &psxM[0x200000];
	psxH = &psxM[0x210000];

    if (Config.Cpu == DYNACORE_DYNAREC) // Lightrec
	{
		if (!lightrec_mmap_inited)
        {

			/* Memory-map the allocated buffers */
			if (lightrec_mmap(psxM, 0x0, 0x200000)
				|| lightrec_mmap(psxM, 0x200000, 0x200000)
				|| lightrec_mmap(psxM, 0x400000, 0x200000)
				|| lightrec_mmap(psxM, 0x600000, 0x200000)) {
				SysMessage(_("Error mapping RAM"));
			}

			if (lightrec_mmap(psxR, 0x1fc00000, 0x80000))
				SysMessage(_("Error mapping BIOS"));

			if (lightrec_mmap(psxM + 0x210000, 0x1f800000, 0x3000))
				SysMessage(_("Error mapping scratch/IO"));

			if (lightrec_mmap(recBuffer, 0x800000, BUF_SIZE))
				SysMessage(_("Error mapping scratch/IO"));

			lightrec_mmap_inited = true;
        }
	}

	memset(psxMemRLUT, 0, 0x10000 * sizeof(void*));
	memset(psxMemWLUT, 0, 0x10000 * sizeof(void*));

// MemR
	for (i=0; i<0x80; i++) psxMemRLUT[i + 0x0000] = (u8*)&psxM[(i & 0x1f) << 16];

	memcpy(psxMemRLUT + 0x8000, psxMemRLUT, 0x80 * sizeof(void*));
	memcpy(psxMemRLUT + 0xa000, psxMemRLUT, 0x80 * sizeof(void*));

	psxMemRLUT[0x1f00] = (u8 *)psxP;
	psxMemRLUT[0x1f80] = (u8 *)psxH;

	for (i = 0; i < 0x08; i++) psxMemRLUT[i + 0x1fc0] = (u8 *)&psxR[i << 16];

	memcpy(psxMemRLUT + 0x9fc0, psxMemRLUT + 0x1fc0, 0x08 * sizeof(void *));
	memcpy(psxMemRLUT + 0xbfc0, psxMemRLUT + 0x1fc0, 0x08 * sizeof(void *));
// MemW
	for (i=0; i<0x80; i++) psxMemWLUT[i + 0x0000] = (u8*)&psxM[(i & 0x1f) << 16];
	memcpy(psxMemWLUT + 0x8000, psxMemWLUT, 0x80 * sizeof(void*));
	memcpy(psxMemWLUT + 0xa000, psxMemWLUT, 0x80 * sizeof(void*));

	// Don't allow writes to PIO Expansion region (psxP) to take effect.
	// NOTE: Not sure if this is needed to fix any games but seems wise,
	//       seeing as some games do read from PIO as part of copy-protection
	//       check. (See fix in psxMemReset() regarding psxP region reads).
	psxMemWLUT[0x1f00] = 0;
	psxMemWLUT[0x1f80] = (u8 *)psxH;

    // enable HID2(PSE)
    u32 hid2 = mfhid2();
    mthid2(hid2 | 0x20000000);

    //CAST_SetGQR(GQR2, GQR_TYPE_S16, 0);
    CAST_SetGQR(GQR3, GQR_TYPE_S16, 0);
    CAST_SetGQR(GQR4, GQR_TYPE_U16, 0); // set GQR4 load u16 => float
    CAST_SetGQR(GQR5, GQR_TYPE_S16, 12); // set GQR4 load s16 => float >> 12
    CAST_SetGQR(GQR6, GQR_TYPE_S16, 8); // set GQR4 load s16 => float >> 8
    //CAST_SetGQR(GQR7, GQR_TYPE_S16, 0);

    //DCEnable();
    //ICEnable();

	return 0;
}

void psxMemReset() {
    //printf("BIOS file %s\n",biosFile->name);
  int temp;
	memset(psxM, 0, 0x00200000);
	memset(psxP, 0xff, 0x00010000);
  memset(psxR, 0, 0x80000);
	// upd xjsxjs197 start
  /*if(!biosFile || (biosDevice == BIOSDEVICE_HLE)) {
    Config.HLE = BIOS_HLE;
    return;
  }
  else {
    biosFile->offset = 0; //must reset otherwise the if statement will fail!
  	if(biosFile_readFile(biosFile, &temp, 4) == 4) {  //bios file exists
  	  biosFile->offset = 0;
  		if(biosFile_readFile(biosFile, psxR, 0x80000) != 0x80000) { //failed size
  		  //printf("Using HLE\n");
  		  Config.HLE = BIOS_HLE;
  	  }
  		else {
    		//printf("Using BIOS file %s\n",biosFile->name);
  		  Config.HLE = BIOS_USER_DEFINED;
  	  }
  	}
  	else {  //bios fails to open
    	Config.HLE = BIOS_HLE;
  	}
	}*/

	biosFile->offset = 0; //must reset otherwise the if statement will fail!
    if (biosFile_readFile(biosFile, &temp, 4) == 4) {  //bios file exists
       biosFile->offset = 0;
	   //printf("BIOS file exists\n");
	   if (biosFile_readFile(biosFile, psxR, 0x80000) != 0x80000) { //failed size
	       Config.HLE = BIOS_HLE;
	       //printf("BIOS file read %x\n", temp);
       }
    }

	if(!biosFile || (biosDevice == BIOSDEVICE_HLE)) {
        Config.HLE = BIOS_HLE;
    }
	else {
	    Config.HLE = BIOS_USER_DEFINED;
	}
	// upd xjsxjs197 end
}

void psxMemShutdown() {
/*	free(psxM);
	free(psxR);
	free(psxMemRLUT);
	free(psxMemWLUT);*/
}

static int writeok=1;

void psxMemOnIsolate(int enable)
{
	if (enable) {
		memset(psxMemWLUT + 0x0000, (int)(uintptr_t)INVALID_PTR, 0x80 * sizeof(void *));
		memset(psxMemWLUT + 0x8000, (int)(uintptr_t)INVALID_PTR, 0x80 * sizeof(void *));
		//memset(psxMemWLUT + 0xa000, (int)(uintptr_t)INVALID_PTR, 0x80 * sizeof(void *));
	} else {
		int i;
		for (i = 0; i < 0x80; i++)
			psxMemWLUT[i + 0x0000] = (void *)&psxM[(i & 0x1f) << 16];
		memcpy(psxMemWLUT + 0x8000, psxMemWLUT, 0x80 * sizeof(void *));
		memcpy(psxMemWLUT + 0xa000, psxMemWLUT, 0x80 * sizeof(void *));
	}
	psxCpu->Notify(enable ? R3000ACPU_NOTIFY_CACHE_ISOLATED
			: R3000ACPU_NOTIFY_CACHE_UNISOLATED, NULL);
}

u8 psxMemRead8(u32 mem) {
	u32 t;

	t = mem >> 16;
	if (t == 0x1f80 || t == 0x9f80 || t == 0xbf80) {
		if ((mem & 0xffff) < 0x400)
			return psxHu8(mem);
		else
			return psxHwRead8(mem);
	} else {
		char *p = (char *)(psxMemRLUT[t]);
		if (p != NULL) {
			return *(u8 *)(p + (mem & 0xffff));
		} else {
#ifdef PSXMEM_LOG
			PSXMEM_LOG("err lb %8.8lx\n", mem);
#endif
			return 0xFF;
		}
	}
}

u16 psxMemRead16(u32 mem) {
	u32 t;

	t = mem >> 16;
	if (t == 0x1f80 || t == 0x9f80 || t == 0xbf80) {
		if ((mem & 0xffff) < 0x400)
            // upd xjsxjs197 start
			//return psxHu16(mem);
			return LOAD_SWAP16p(psxHAddr(mem));
            // upd xjsxjs197 end
		else
			return psxHwRead16(mem);
	} else {
		char *p = (char *)(psxMemRLUT[t]);
		if (p != NULL) {
            // upd xjsxjs197 start
			//return SWAPu16(*(u16 *)(p + (mem & 0xffff)));
			return LOAD_SWAP16p(p + (mem & 0xffff));
            // upd xjsxjs197 end
		} else {
#ifdef PSXMEM_LOG
			PSXMEM_LOG("err lh %8.8lx\n", mem);
#endif
			return 0xFFFF;
		}
	}
}

u32 psxMemRead32(u32 mem) {
	u32 t;

	t = mem >> 16;
	if (t == 0x1f80 || t == 0x9f80 || t == 0xbf80) {
		if ((mem & 0xffff) < 0x400)
            // upd xjsxjs197 start
			//return psxHu32(mem);
			return LOAD_SWAP32p(psxHAddr(mem));
            // upd xjsxjs197 end
		else
			return psxHwRead32(mem);
	} else {
		char *p = (char *)(psxMemRLUT[t]);
		if (p != NULL) {
            // upd xjsxjs197 start
			//return SWAPu32(*(u32 *)(p + (mem & 0xffff)));
			return LOAD_SWAP32p(p + (mem & 0xffff));
            // upd xjsxjs197 end
		} else {
#ifdef PSXMEM_LOG
			if (writeok) { PSXMEM_LOG("err lw %8.8lx\n", mem); }
#endif
			return 0xFFFFFFFF;
		}
	}
}

void psxMemWrite8(u32 mem, u32 value) {
	u32 t;

	t = mem >> 16;
	if (t == 0x1f80 || t == 0x9f80 || t == 0xbf80) {
		if ((mem & 0xffff) < 0x400)
			psxHu8(mem) = value;
		else
			psxHwWrite8(mem, value);
	} else {
		char *p = (char *)(psxMemWLUT[t]);
		if (p != NULL) {
			*(u8  *)(p + (mem & 0xffff)) = value;
//#ifdef PSXREC
			psxCpu->Clear((mem&(~3)), 1);
//#endif
		} else {
#ifdef PSXMEM_LOG
			PSXMEM_LOG("err sb %8.8lx\n", mem);
#endif
		}
	}
}

void psxMemWrite16(u32 mem, u32 value) {
	u32 t;

	t = mem >> 16;
	if (t == 0x1f80 || t == 0x9f80 || t == 0xbf80) {
		if ((mem & 0xffff) < 0x400)
            // upd xjsxjs197 start
			//psxHu16ref(mem) = SWAPu16(value);
			STORE_SWAP16p(psxHAddr(mem), value);
            // upd xjsxjs197 end
		else
			psxHwWrite16(mem, value);
	} else {
		char *p = (char *)(psxMemWLUT[t]);
		if (p != NULL) {
            // upd xjsxjs197 start
			//*(u16 *)(p + (mem & 0xffff)) = SWAPu16(value);
			STORE_SWAP16p((p + (mem & 0xffff)), value);
            // upd xjsxjs197 end
//#ifdef PSXREC
			psxCpu->Clear((mem & (~3)), 1);
//#endif
		} else {
#ifdef PSXMEM_LOG
			PSXMEM_LOG("err sh %8.8lx\n", mem);
#endif
		}
	}
}

void psxMemWrite32(u32 mem, u32 value) {
	u32 t;

//	if ((mem&0x1fffff) == 0x71E18 || value == 0x48088800) SysPrintf("t2fix!!\n");
	t = mem >> 16;
	if (t == 0x1f80 || t == 0x9f80 || t == 0xbf80) {
		if ((mem & 0xffff) < 0x400)
            // upd xjsxjs197 start
			//psxHu32ref(mem) = SWAPu32(value);
			STORE_SWAP32p(psxHAddr(mem), value);
            // upd xjsxjs197 end
		else
			psxHwWrite32(mem, value);
	} else {
		char *p = (char *)(psxMemWLUT[t]);
		if (p != NULL) {
            // upd xjsxjs197 start
			//*(u32 *)(p + (mem & 0xffff)) = SWAPu32(value);
			STORE_SWAP32p((p + (mem & 0xffff)), value);
            // upd xjsxjs197 end
//#ifdef PSXREC
			psxCpu->Clear(mem, 1);
//#endif
		} else {
			if (mem != 0xfffe0130) {
//#ifdef PSXREC
				if (!writeok)
					psxCpu->Clear(mem, 1);
//#endif

#ifdef PSXMEM_LOG
				if (writeok) { PSXMEM_LOG("err sw %8.8lx\n", mem); }
#endif
			} else {
				int i;

				switch (value) {
					case 0x800: case 0x804:
						if (writeok == 0) break;
						writeok = 0;
						memset(psxMemWLUT + 0x0000, 0, 0x80 * sizeof(void *));
						memset(psxMemWLUT + 0x8000, 0, 0x80 * sizeof(void *));
						memset(psxMemWLUT + 0xa000, 0, 0x80 * sizeof(void *));

						psxRegs.ICache_valid = FALSE;
						break;
					case 0x00: case 0x1e988:
						if (writeok == 1) break;
						writeok = 1;
						for (i = 0; i < 0x80; i++) psxMemWLUT[i + 0x0000] = (void *)&psxM[(i & 0x1f) << 16];
						memcpy(psxMemWLUT + 0x8000, psxMemWLUT, 0x80 * sizeof(void *));
						memcpy(psxMemWLUT + 0xa000, psxMemWLUT, 0x80 * sizeof(void *));
						break;
					default:
#ifdef PSXMEM_LOG
						PSXMEM_LOG("unk %8.8lx = %x\n", mem, value);
#endif
						break;
				}
			}
		}
	}
}

void *psxMemPointer(u32 mem) {
	u32 t;

	t = mem >> 16;
	if (t == 0x1f80 || t == 0x9f80 || t == 0xbf80) {
		if ((mem & 0xffff) < 0x400)
			return (void *)&psxH[mem];
		else
			return NULL;
	} else {
		char *p = (char *)(psxMemWLUT[t]);
		if (p != NULL) {
			return (void *)(p + (mem & 0xffff));
		}
		return NULL;
	}
}
