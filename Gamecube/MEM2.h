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

// We want 128KB for our MEMCARD 1
#define MCD1_SIZE     (128*KB)
#define MCD1_LO       (MEM2_LO)
#define MCD1_HI       (MCD1_LO + MCD1_SIZE)

// We want 128KB for our MEMCARD 2
#define MCD2_SIZE     (128*KB)
#define MCD2_LO       (MCD1_HI)
#define MCD2_HI       (MCD2_LO + MCD2_SIZE)

// We want 256KB for fontFont
#define FONT_SIZE (256*KB)
#define FONT_LO   (MCD2_HI)
#define FONT_HI   (FONT_LO + FONT_SIZE)

// We want 20MB for max font
#define CN_FONT_SIZE (19*MB)
#define CN_FONT_LO   (FONT_HI)
#define CN_FONT_HI   (CN_FONT_LO + CN_FONT_SIZE)

// We want 20MB for the recompiled blocks
#define RECMEM2_SIZE (20*MB)
#define RECMEM2_LO   (CN_FONT_HI)
#define RECMEM2_HI   (RECMEM2_LO + RECMEM2_SIZE)

#endif
