#include "gpuVramTilingInternal.h"

#include <string.h>

uint16_t GlesVramPsx15ToGXRGB5A3(uint16_t psx)
{
    return (uint16_t)(0x8000u |
                      ((psx & 0x001fu) << 10) |
                      (psx & 0x03e0u) |
                      ((psx >> 10) & 0x001fu));
}

uint16_t GlesVramGXRGB5A3ToPsx15(uint16_t gx)
{
    uint16_t r;
    uint16_t g;
    uint16_t b;

    if (gx & 0x8000u)
    {
        r = (gx >> 10) & 0x1fu;
        g = (gx >> 5) & 0x1fu;
        b = gx & 0x1fu;
    }
    else
    {
        r = (gx >> 8) & 0x0fu;
        g = (gx >> 4) & 0x0fu;
        b = gx & 0x0fu;
        r = (uint16_t)((r << 1) | (r >> 3));
        g = (uint16_t)((g << 1) | (g >> 3));
        b = (uint16_t)((b << 1) | (b >> 3));
    }
    return (uint16_t)(r | (g << 5) | (b << 10));
}

size_t GlesVramRGB5A3Offset(int textureWidth, int x, int y)
{
    size_t blocksPerRow = (size_t)textureWidth >> 2;
    size_t block = ((size_t)y >> 2) * blocksPerRow +
                   ((size_t)x >> 2);
    return (block << 5) +
           (((((size_t)y & 3u) << 2) + ((size_t)x & 3u)) << 1);
}

void GlesVramManagerReset(GlesVramManagerState *state)
{
    int x;
    int y;

    if (state == NULL)
        return;

    memset(state, 0, sizeof(*state));
    state->writeSequence = 1;
    state->activeTile = GLES_VRAM_TILE_NONE;

    /* GP1 reset does not clear PS VRAM.  Until a later stage uploads a block,
     * psxVuw is the single authoritative copy. */
    for (y = 0; y < GLES_VRAM_BLOCKS_Y; y++)
    {
        for (x = 0; x < GLES_VRAM_BLOCKS_X; x++)
        {
            state->block[y][x].cpuSequence = 1;
            state->block[y][x].cacheSequence = 1;
            state->block[y][x].flags = GLES_VRAM_BLOCK_CPU_VALID;
        }
    }
}

unsigned int GlesVramManagerSelectTile(GlesVramManagerState *state,
                                      GlesVramTileId tile)
{
    unsigned int actions = 0;

    if (state == NULL ||
        (tile != GLES_VRAM_TILE_LEFT && tile != GLES_VRAM_TILE_RIGHT))
        return 0;

    if (state->activeTile == tile)
        return 0;

    if (state->activeTile != GLES_VRAM_TILE_NONE && state->efbDirty)
    {
        actions |= GLES_VRAM_SWITCH_SAVE;
        state->backingValid[state->activeTile] = 1;
        state->saveCount++;
    }

    if (state->backingValid[tile])
    {
        actions |= GLES_VRAM_SWITCH_RESTORE;
        state->restoreCount++;
    }
    else
    {
        actions |= GLES_VRAM_SWITCH_CLEAR;
    }

    state->activeTile = tile;
    state->efbDirty = 0;
    return actions;
}

void GlesVramManagerMarkActiveDirty(GlesVramManagerState *state)
{
    if (state != NULL && state->activeTile != GLES_VRAM_TILE_NONE)
        state->efbDirty = 1;
}

void GlesVramManagerDiscardActive(GlesVramManagerState *state)
{
    if (state == NULL)
        return;
    state->activeTile = GLES_VRAM_TILE_NONE;
    state->efbDirty = 0;
}

GlesVramTileId GlesVramManagerSaveAndDiscardActive(
    GlesVramManagerState *state)
{
    GlesVramTileId tile;

    if (state == NULL)
        return GLES_VRAM_TILE_NONE;

    tile = state->activeTile;
    if (tile != GLES_VRAM_TILE_NONE && state->efbDirty)
    {
        state->backingValid[tile] = 1;
        state->saveCount++;
    }
    else
    {
        tile = GLES_VRAM_TILE_NONE;
    }
    state->activeTile = GLES_VRAM_TILE_NONE;
    state->efbDirty = 0;
    return tile;
}

GlesVramTileId GlesVramManagerPersistActive(
    GlesVramManagerState *state)
{
    GlesVramTileId tile;

    if (state == NULL)
        return GLES_VRAM_TILE_NONE;

    tile = state->activeTile;
    if (tile == GLES_VRAM_TILE_NONE || !state->efbDirty)
        return GLES_VRAM_TILE_NONE;

    state->backingValid[tile] = 1;
    state->saveCount++;
    state->efbDirty = 0;
    return tile;
}

uint64_t GlesVramManagerBeginWrite(GlesVramManagerState *state)
{
    if (state == NULL)
        return 0;

    state->writeSequence++;
    if (state->writeSequence == 0)
        state->writeSequence = 1;
    return state->writeSequence;
}

static void GlesVramManagerMarkWrite(GlesVramManagerState *state,
                                    const GlesVramRect *rect,
                                    uint64_t sequence,
                                    int gpuWrite)
{
    GlesVramRectPiece pieces[GLES_VRAM_MAX_WRAP_PIECES];
    int pieceCount;
    int pieceIndex;

    if (state == NULL || rect == NULL || sequence == 0)
        return;

    pieceCount = GlesVramSplitWrappedRect(rect, pieces);
    for (pieceIndex = 0; pieceIndex < pieceCount; pieceIndex++)
    {
        const GlesVramRect *piece = &pieces[pieceIndex].rect;
        int bx0 = piece->x / GLES_VRAM_BLOCK_WIDTH;
        int by0 = piece->y / GLES_VRAM_BLOCK_HEIGHT;
        int bx1 = (piece->x + piece->width - 1) / GLES_VRAM_BLOCK_WIDTH;
        int by1 = (piece->y + piece->height - 1) / GLES_VRAM_BLOCK_HEIGHT;
        int bx;
        int by;

        for (by = by0; by <= by1; by++)
        {
            for (bx = bx0; bx <= bx1; bx++)
            {
                GlesVramBlockState *block = &state->block[by][bx];
                if (gpuWrite)
                {
                    block->gpuSequence = sequence;
                    block->cacheSequence = sequence;
                    block->flags = GLES_VRAM_BLOCK_GPU_VALID;
                }
                else
                {
                    block->cpuSequence = sequence;
                    block->cacheSequence = sequence;
                    if (block->gpuSequence == sequence)
                        block->flags = GLES_VRAM_BLOCK_CPU_VALID |
                                       GLES_VRAM_BLOCK_GPU_VALID;
                    else
                        block->flags = GLES_VRAM_BLOCK_CPU_VALID;
                }
            }
        }
    }
}

void GlesVramManagerMarkCpuWrite(GlesVramManagerState *state,
                                 const GlesVramRect *rect,
                                 uint64_t sequence)
{
    GlesVramManagerMarkWrite(state, rect, sequence, 0);
}

void GlesVramManagerMarkGpuWrite(GlesVramManagerState *state,
                                 const GlesVramRect *rect,
                                 uint64_t sequence)
{
    GlesVramManagerMarkWrite(state, rect, sequence, 1);
}

static int GlesVramManagerValidBlock(int blockX, int blockY)
{
    return blockX >= 0 && blockX < GLES_VRAM_BLOCKS_X &&
           blockY >= 0 && blockY < GLES_VRAM_BLOCKS_Y;
}

int GlesVramManagerBlockNeedsCpuUpload(
    const GlesVramManagerState *state, int blockX, int blockY)
{
    const GlesVramBlockState *block;

    if (state == NULL || !GlesVramManagerValidBlock(blockX, blockY))
        return 0;
    block = &state->block[blockY][blockX];
    return (block->flags & GLES_VRAM_BLOCK_CPU_VALID) != 0 &&
           ((block->flags & GLES_VRAM_BLOCK_GPU_VALID) == 0 ||
           block->cpuSequence > block->gpuSequence);
}

int GlesVramManagerRectNeedsCpuUpload(
    const GlesVramManagerState *state, const GlesVramRect *rect)
{
    GlesVramRectPiece pieces[GLES_VRAM_MAX_WRAP_PIECES];
    int pieceCount;
    int pieceIndex;

    if (state == NULL || rect == NULL)
        return 0;
    pieceCount = GlesVramSplitWrappedRect(rect, pieces);
    for (pieceIndex = 0; pieceIndex < pieceCount; pieceIndex++)
    {
        const GlesVramRect *piece = &pieces[pieceIndex].rect;
        int blockX0 = piece->x / GLES_VRAM_BLOCK_WIDTH;
        int blockX1 = (piece->x + piece->width - 1) /
                      GLES_VRAM_BLOCK_WIDTH;
        int blockY0 = piece->y / GLES_VRAM_BLOCK_HEIGHT;
        int blockY1 = (piece->y + piece->height - 1) /
                      GLES_VRAM_BLOCK_HEIGHT;
        int blockX;
        int blockY;

        for (blockY = blockY0; blockY <= blockY1; blockY++)
        {
            for (blockX = blockX0; blockX <= blockX1; blockX++)
            {
                if (GlesVramManagerBlockNeedsCpuUpload(
                        state, blockX, blockY))
                    return 1;
            }
        }
    }
    return 0;
}

int GlesVramManagerCommitCpuUpload(GlesVramManagerState *state,
                                   int blockX, int blockY,
                                   uint64_t capturedCpuSequence)
{
    GlesVramBlockState *block;

    if (state == NULL || !GlesVramManagerValidBlock(blockX, blockY))
        return 0;
    block = &state->block[blockY][blockX];
    if ((block->flags & GLES_VRAM_BLOCK_CPU_VALID) == 0 ||
        block->cpuSequence != capturedCpuSequence)
        return 0;

    block->gpuSequence = capturedCpuSequence;
    block->flags = GLES_VRAM_BLOCK_CPU_VALID |
                   GLES_VRAM_BLOCK_GPU_VALID;
    return 1;
}

int GlesVramManagerResolveGpuBlock(GlesVramManagerState *state,
                                   int blockX, int blockY,
                                   uint64_t capturedGpuSequence)
{
    GlesVramBlockState *block;

    if (state == NULL || !GlesVramManagerValidBlock(blockX, blockY))
        return 0;
    block = &state->block[blockY][blockX];
    if ((block->flags & GLES_VRAM_BLOCK_GPU_VALID) == 0 ||
        block->gpuSequence != capturedGpuSequence ||
        block->cpuSequence > capturedGpuSequence)
        return 0;

    block->cpuSequence = capturedGpuSequence;
    block->flags = GLES_VRAM_BLOCK_CPU_VALID |
                   GLES_VRAM_BLOCK_GPU_VALID;
    return 1;
}

static int GlesVramRectsOverlap(const GlesVramRect *first,
                                const GlesVramRect *second)
{
    GlesVramRectPiece firstPiece[GLES_VRAM_MAX_WRAP_PIECES];
    GlesVramRectPiece secondPiece[GLES_VRAM_MAX_WRAP_PIECES];
    int firstCount;
    int secondCount;
    int firstIndex;
    int secondIndex;

    firstCount = GlesVramSplitWrappedRect(first, firstPiece);
    secondCount = GlesVramSplitWrappedRect(second, secondPiece);
    for (firstIndex = 0; firstIndex < firstCount; firstIndex++)
    {
        const GlesVramRect *a = &firstPiece[firstIndex].rect;
        for (secondIndex = 0; secondIndex < secondCount; secondIndex++)
        {
            const GlesVramRect *b = &secondPiece[secondIndex].rect;
            if (a->x < b->x + b->width && b->x < a->x + a->width &&
                a->y < b->y + b->height && b->y < a->y + a->height)
                return 1;
        }
    }
    return 0;
}

void GlesVramFrontPresentationReset(
    GlesVramFrontPresentationState *state)
{
    if (state != NULL)
        memset(state, 0, sizeof(*state));
}

void GlesVramFrontPresentationMarkWrite(
    GlesVramFrontPresentationState *state,
    const GlesVramRect *writeRect,
    const GlesVramRect *frontDisplay)
{
    if (state == NULL || writeRect == NULL || frontDisplay == NULL ||
        !GlesVramRectsOverlap(writeRect, frontDisplay))
        return;

    state->pendingDisplay = *frontDisplay;
    state->pending = 1;
}

int GlesVramFrontPresentationSelect(
    GlesVramFrontPresentationState *state,
    const GlesVramRect *requestedDisplay,
    GlesVramRect *selectedDisplay)
{
    int usedFront;

    if (state == NULL || requestedDisplay == NULL || selectedDisplay == NULL)
        return 0;
    usedFront = state->pending != 0;
    *selectedDisplay = usedFront ? state->pendingDisplay : *requestedDisplay;
    state->pending = 0;
    return usedFront;
}

static uint16_t GlesVramReadLittle16(const unsigned short *cpuVram,
                                     int x, int y)
{
    const unsigned char *bytes = (const unsigned char *)(cpuVram +
        y * GLES_VRAM_WIDTH + x);
    return (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8);
}

static void GlesVramWriteLittle16(unsigned short *cpuVram,
                                  int x, int y, uint16_t value)
{
    unsigned char *bytes = (unsigned char *)(cpuVram +
        y * GLES_VRAM_WIDTH + x);
    bytes[0] = (unsigned char)value;
    bytes[1] = (unsigned char)(value >> 8);
}

int GlesVramCopyWrapped(unsigned short *cpuVram,
                        const GlesVramRect *source,
                        const GlesVramRect *destination,
                        uint16_t setMask,
                        uint16_t *scratch,
                        size_t scratchPixels)
{
    size_t required;
    size_t index;
    int x;
    int y;

    if (cpuVram == NULL || source == NULL || destination == NULL ||
        scratch == NULL || source->width <= 0 || source->height <= 0 ||
        source->width != destination->width ||
        source->height != destination->height ||
        source->width > GLES_VRAM_WIDTH ||
        source->height > GLES_VRAM_HEIGHT)
        return 0;

    required = (size_t)source->width * (size_t)source->height;
    if (required > scratchPixels)
        return 0;

    index = 0;
    for (y = 0; y < source->height; y++)
    {
        int sourceY = (source->y + y) & (GLES_VRAM_HEIGHT - 1);
        for (x = 0; x < source->width; x++)
        {
            int sourceX = (source->x + x) & (GLES_VRAM_WIDTH - 1);
            scratch[index++] = GlesVramReadLittle16(cpuVram,
                                                    sourceX, sourceY);
        }
    }

    index = 0;
    for (y = 0; y < destination->height; y++)
    {
        int destinationY = (destination->y + y) &
                           (GLES_VRAM_HEIGHT - 1);
        for (x = 0; x < destination->width; x++)
        {
            int destinationX = (destination->x + x) &
                               (GLES_VRAM_WIDTH - 1);
            GlesVramWriteLittle16(cpuVram, destinationX, destinationY,
                                  (uint16_t)(scratch[index++] | setMask));
        }
    }
    return 1;
}
