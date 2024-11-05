/* MEM2.h - MEM2 boundaries for different chunks of memory
   by Mike Slegeir for Mupen64-Wii adapted for WiiSX by emu_kidid
 */

#ifndef MEM2_H
#define MEM2_H

// Define a KiloByte (KB) and a MegaByte (MB)
#define KB (1024)
#define MB (1024*1024)

// MEM2 begins at MEM2_LO, the Starlet's Dedicated Memory begins at MEM2_HI
#define MEM2_LO   ((char*)0x90080000)
#define MEM2_HI   ((char*)0x933E0000)
#define MEM2_SIZE (MEM2_HI - MEM2_LO)

// We want 128KB for our MEMCARD 1
#define MCD1_SIZE     (128*KB)
#define MCD1_LO       (MEM2_LO)
#define MCD1_HI       (MCD1_LO + MCD1_SIZE)

// We want 128KB for our MEMCARD 2
#define MCD2_SIZE     (128*KB)
#define MCD2_LO       (MCD1_HI)
#define MCD2_HI       (MCD2_LO + MCD2_SIZE)

// We want 10MB for max font
#define CN_FONT_SIZE (10*MB)
#define CN_FONT_LO   (MCD2_HI)
#define CN_FONT_HI   (CN_FONT_LO + CN_FONT_SIZE)

// We want 10MB for the recompiled blocks of the 'new' PPC Dynarec
#define RECMEM2_SIZE (10*MB)
#define RECMEM2_LO   (CN_FONT_HI)
#define RECMEM2_HI   (RECMEM2_LO + RECMEM2_SIZE)

// We want 512KB for the SPU buffer
#define SPU_BUF_SIZE (512*KB)
#define SPU_BUF_LO   (RECMEM2_HI)
#define SPU_BUF_HI   (SPU_BUF_LO + SPU_BUF_SIZE)

// We want 4MB for Lightrec code buffer
#define LIGHTREC_BUF_SIZE (4*MB)
#define LIGHTREC_BUF_LO   (SPU_BUF_HI)
#define LIGHTREC_BUF_HI   (LIGHTREC_BUF_LO + LIGHTREC_BUF_SIZE)


#define NEW_MEM2_LO LIGHTREC_BUF_HI

#endif
