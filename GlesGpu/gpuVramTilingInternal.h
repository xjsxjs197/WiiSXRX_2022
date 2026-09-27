#ifndef GPU_VRAM_TILING_INTERNAL_H
#define GPU_VRAM_TILING_INTERNAL_H

#include "gpuVramTiling.h"

#define GLES_VRAM_BLOCK_WIDTH  16
#define GLES_VRAM_BLOCK_HEIGHT 16
#define GLES_VRAM_BLOCKS_X \
    (GLES_VRAM_WIDTH / GLES_VRAM_BLOCK_WIDTH)
#define GLES_VRAM_BLOCKS_Y \
    (GLES_VRAM_HEIGHT / GLES_VRAM_BLOCK_HEIGHT)

#define GLES_VRAM_BLOCK_CPU_VALID 0x01u
#define GLES_VRAM_BLOCK_GPU_VALID 0x02u

#define GLES_VRAM_SWITCH_SAVE    0x01u
#define GLES_VRAM_SWITCH_RESTORE 0x02u
#define GLES_VRAM_SWITCH_CLEAR   0x04u

uint16_t GlesVramPsx15ToGXRGB5A3(uint16_t psx);
uint16_t GlesVramGXRGB5A3ToPsx15(uint16_t gx);
size_t GlesVramRGB5A3Offset(int textureWidth, int x, int y);

typedef struct GlesVramBlockStateTag
{
    uint64_t cpuSequence;
    uint64_t gpuSequence;
    uint64_t cacheSequence;
    unsigned char flags;
} GlesVramBlockState;

/* This part of the manager deliberately has no GX dependency.  Keeping the
 * transition decision here makes the host test exercise the same state
 * changes used by the Wii implementation. */
typedef struct GlesVramManagerStateTag
{
    GlesVramBlockState block[GLES_VRAM_BLOCKS_Y][GLES_VRAM_BLOCKS_X];
    uint64_t writeSequence;
    uint32_t saveCount;
    uint32_t restoreCount;
    GlesVramTileId activeTile;
    unsigned char backingValid[2];
    unsigned char efbDirty;
} GlesVramManagerState;

typedef struct GlesVramFrontPresentationStateTag
{
    GlesVramRect pendingDisplay;
    unsigned char pending;
} GlesVramFrontPresentationState;

void GlesVramManagerReset(GlesVramManagerState *state);
unsigned int GlesVramManagerSelectTile(GlesVramManagerState *state,
                                      GlesVramTileId tile);
void GlesVramManagerMarkActiveDirty(GlesVramManagerState *state);
void GlesVramManagerDiscardActive(GlesVramManagerState *state);
GlesVramTileId GlesVramManagerSaveAndDiscardActive(
    GlesVramManagerState *state);
GlesVramTileId GlesVramManagerPersistActive(
    GlesVramManagerState *state);
uint64_t GlesVramManagerBeginWrite(GlesVramManagerState *state);
void GlesVramManagerMarkCpuWrite(GlesVramManagerState *state,
                                 const GlesVramRect *rect,
                                 uint64_t sequence);
void GlesVramManagerMarkGpuWrite(GlesVramManagerState *state,
                                 const GlesVramRect *rect,
                                 uint64_t sequence);
int GlesVramManagerBlockNeedsCpuUpload(
    const GlesVramManagerState *state, int blockX, int blockY);
int GlesVramManagerRectNeedsCpuUpload(
    const GlesVramManagerState *state, const GlesVramRect *rect);
int GlesVramManagerCommitCpuUpload(GlesVramManagerState *state,
                                   int blockX, int blockY,
                                   uint64_t capturedCpuSequence);
int GlesVramManagerResolveGpuBlock(GlesVramManagerState *state,
                                   int blockX, int blockY,
                                   uint64_t capturedGpuSequence);
void GlesVramFrontPresentationReset(
    GlesVramFrontPresentationState *state);
void GlesVramFrontPresentationMarkWrite(
    GlesVramFrontPresentationState *state,
    const GlesVramRect *writeRect,
    const GlesVramRect *frontDisplay);
int GlesVramFrontPresentationSelect(
    GlesVramFrontPresentationState *state,
    const GlesVramRect *requestedDisplay,
    GlesVramRect *selectedDisplay);
int GlesVramCopyWrapped(unsigned short *cpuVram,
                        const GlesVramRect *source,
                        const GlesVramRect *destination,
                        uint16_t setMask,
                        uint16_t *scratch,
                        size_t scratchPixels);

#endif
