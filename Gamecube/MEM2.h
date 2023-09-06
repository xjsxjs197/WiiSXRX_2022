/* MEM2.h - MEM2 boundaries for different chunks of memory
   by Mike Slegeir for Mupen64-Wii adapted for WiiSX by emu_kidid
 */

#ifndef MEM2_H
#define MEM2_H

// Define a MegaByte
#define KB (1024)
#define MB (1024*1024)

// MEM2 begins at MEM2_LO, the Starlet's Dedicated Memory begins at MEM2_HI
#define MEM2_LO   ((char*)0x90080000)
#define MEM2_HI   ((char*)0x933E0000)
#define MEM2_SIZE (MEM2_HI - MEM2_LO)

// We want 5MB for our IOS58 KernelDst
#define KERNEL_DST_SIZE (5*MB)
#define KERNEL_DST_LO   (MEM2_LO)
#define KERNEL_DST_HI   (KERNEL_DST_LO + KERNEL_DST_SIZE)

// We want 5MB for our KernelReadBuf
#define KERNEL_BUF_SIZE (5*MB)
#define KERNEL_BUF_LO   (KERNEL_DST_HI)
#define KERNEL_BUF_HI   (KERNEL_BUF_LO + KERNEL_BUF_SIZE)

// We want 128KB for our MEMCARD 1
#define MCD1_SIZE     (128*KB)
#define MCD1_LO       (KERNEL_BUF_HI)
#define MCD1_HI       (MCD1_LO + MCD1_SIZE)

// We want 128KB for our MEMCARD 2
#define MCD2_SIZE     (128*KB)
#define MCD2_LO       (MCD1_HI)
#define MCD2_HI       (MCD2_LO + MCD2_SIZE)

// We want 20MB for font for various languages
#define CN_FONT_SIZE (20*MB)
#define CN_FONT_LO   (MCD2_HI)
#define CN_FONT_HI   (CN_FONT_LO + CN_FONT_SIZE)

// We want 15MB for the recompiled blocks
#define RECMEM2_SIZE (15*MB)
#define RECMEM2_LO   (CN_FONT_HI)
#define RECMEM2_HI   (RECMEM2_LO + RECMEM2_SIZE)

#endif
