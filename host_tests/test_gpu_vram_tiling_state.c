#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../GlesGpu/gpuVramTilingInternal.h"

static void ApplySwitch(GlesVramManagerState *state,
                        GlesVramTileId next,
                        uint32_t *efbHash,
                        uint32_t backingHash[2])
{
    GlesVramTileId old = state->activeTile;
    unsigned int actions = GlesVramManagerSelectTile(state, next);

    if (actions & GLES_VRAM_SWITCH_SAVE)
    {
        assert(old == GLES_VRAM_TILE_LEFT || old == GLES_VRAM_TILE_RIGHT);
        backingHash[old] = *efbHash;
    }
    if (actions & GLES_VRAM_SWITCH_RESTORE)
        *efbHash = backingHash[next];
    else if (actions & GLES_VRAM_SWITCH_CLEAR)
        *efbHash = 0;
}

static void TestThousandRoundTrips(void)
{
    GlesVramManagerState state;
    uint32_t backingHash[2] = {0, 0};
    uint32_t efbHash = 0;
    unsigned char initialBlocks[sizeof(state.block)];
    int i;

    GlesVramManagerReset(&state);
    memcpy(initialBlocks, state.block, sizeof(initialBlocks));

    ApplySwitch(&state, GLES_VRAM_TILE_LEFT, &efbHash, backingHash);
    assert(efbHash == 0);
    efbHash = 0x13579bdfu;
    GlesVramManagerMarkActiveDirty(&state);

    ApplySwitch(&state, GLES_VRAM_TILE_RIGHT, &efbHash, backingHash);
    assert(backingHash[GLES_VRAM_TILE_LEFT] == 0x13579bdfu);
    efbHash = 0x2468ace0u;
    GlesVramManagerMarkActiveDirty(&state);

    ApplySwitch(&state, GLES_VRAM_TILE_LEFT, &efbHash, backingHash);
    assert(efbHash == 0x13579bdfu);

    for (i = 0; i < 1000; i++)
    {
        GlesVramTileId next = state.activeTile == GLES_VRAM_TILE_LEFT ?
                              GLES_VRAM_TILE_RIGHT : GLES_VRAM_TILE_LEFT;
        uint32_t expected = next == GLES_VRAM_TILE_LEFT ?
                            0x13579bdfu : 0x2468ace0u;
        GlesVramManagerMarkActiveDirty(&state);
        ApplySwitch(&state, next, &efbHash, backingHash);
        assert(efbHash == expected);
    }

    /* Tile copies are transport only; they must not advance or mutate block
     * ownership generations. */
    assert(state.writeSequence == 1);
    assert(memcmp(initialBlocks, state.block, sizeof(initialBlocks)) == 0);
    assert(state.saveCount == 1002u);
    assert(state.restoreCount == 1001u);
}

static void TestBlockSequenceAndWrap(void)
{
    GlesVramManagerState state;
    GlesVramRect rect = {1000, 500, 40, 20};
    uint64_t sequence;
    int gpuBlocks = 0;
    int x;
    int y;

    GlesVramManagerReset(&state);
    sequence = GlesVramManagerBeginWrite(&state);
    assert(sequence == 2);
    GlesVramManagerMarkGpuWrite(&state, &rect, sequence);

    for (y = 0; y < GLES_VRAM_BLOCKS_Y; y++)
    {
        for (x = 0; x < GLES_VRAM_BLOCKS_X; x++)
        {
            const GlesVramBlockState *block = &state.block[y][x];
            if (block->flags == GLES_VRAM_BLOCK_GPU_VALID)
            {
                assert(block->gpuSequence == sequence);
                gpuBlocks++;
            }
            else
            {
                assert(block->flags == GLES_VRAM_BLOCK_CPU_VALID);
                assert(block->cpuSequence == 1);
            }
        }
    }
    assert(gpuBlocks == 6);

    sequence = GlesVramManagerBeginWrite(&state);
    GlesVramManagerMarkCpuWrite(&state, &rect, sequence);
    assert(state.block[31][63].flags == GLES_VRAM_BLOCK_CPU_VALID);
    assert(state.block[31][63].cpuSequence == sequence);
    assert(state.block[0][0].flags == GLES_VRAM_BLOCK_CPU_VALID);
    assert(state.block[0][0].cpuSequence == sequence);

    /* A software mirror written by the same Fill generation remains valid
     * together with the GPU tile copy. */
    GlesVramManagerMarkGpuWrite(&state, &rect, sequence);
    GlesVramManagerMarkCpuWrite(&state, &rect, sequence);
    assert(state.block[31][63].flags ==
           (GLES_VRAM_BLOCK_CPU_VALID | GLES_VRAM_BLOCK_GPU_VALID));
}

static void TestLazyUploadAndStaleCompletion(void)
{
    GlesVramManagerState state;
    GlesVramRect blockRect = {640, 32, 16, 16};
    uint64_t gpuSequence;
    uint64_t firstCpuSequence;
    uint64_t secondCpuSequence;

    GlesVramManagerReset(&state);
    gpuSequence = GlesVramManagerBeginWrite(&state);
    GlesVramManagerMarkGpuWrite(&state, &blockRect, gpuSequence);
    assert(!GlesVramManagerBlockNeedsCpuUpload(&state, 40, 2));

    assert(GlesVramManagerResolveGpuBlock(&state, 40, 2,
                                          gpuSequence));
    assert(state.block[2][40].cpuSequence == gpuSequence);
    assert(state.block[2][40].flags ==
           (GLES_VRAM_BLOCK_CPU_VALID | GLES_VRAM_BLOCK_GPU_VALID));

    firstCpuSequence = GlesVramManagerBeginWrite(&state);
    GlesVramManagerMarkCpuWrite(&state, &blockRect, firstCpuSequence);
    assert(GlesVramManagerBlockNeedsCpuUpload(&state, 40, 2));

    /* A newer A0 arrives before an older queued upload is published.  The
     * older completion must not overwrite the newer CPU generation. */
    secondCpuSequence = GlesVramManagerBeginWrite(&state);
    GlesVramManagerMarkCpuWrite(&state, &blockRect, secondCpuSequence);
    assert(!GlesVramManagerCommitCpuUpload(&state, 40, 2,
                                           firstCpuSequence));
    assert(GlesVramManagerBlockNeedsCpuUpload(&state, 40, 2));
    assert(GlesVramManagerCommitCpuUpload(&state, 40, 2,
                                          secondCpuSequence));
    assert(!GlesVramManagerBlockNeedsCpuUpload(&state, 40, 2));
    assert(state.block[2][40].gpuSequence == secondCpuSequence);
    assert(state.block[2][40].cacheSequence == secondCpuSequence);
}

static void TestPendingCpuUploadRect(void)
{
    GlesVramManagerState state;
    GlesVramRect write = {1000, 500, 40, 20};
    GlesVramRect hitRightBottom = {1008, 496, 16, 16};
    GlesVramRect hitWrappedTop = {0, 0, 16, 16};
    GlesVramRect miss = {320, 240, 32, 32};
    uint64_t sequence;

    GlesVramManagerReset(&state);

    /* Establish an all-GPU-current baseline so only the test write remains
     * pending. */
    sequence = GlesVramManagerBeginWrite(&state);
    {
        GlesVramRect full = {0, 0, GLES_VRAM_WIDTH, GLES_VRAM_HEIGHT};
        GlesVramManagerMarkGpuWrite(&state, &full, sequence);
    }
    assert(!GlesVramManagerRectNeedsCpuUpload(&state, &write));

    sequence = GlesVramManagerBeginWrite(&state);
    GlesVramManagerMarkCpuWrite(&state, &write, sequence);
    assert(GlesVramManagerRectNeedsCpuUpload(&state, &hitRightBottom));
    assert(GlesVramManagerRectNeedsCpuUpload(&state, &hitWrappedTop));
    assert(!GlesVramManagerRectNeedsCpuUpload(&state, &miss));
}

static void TestPersistActive(void)
{
    GlesVramManagerState state;

    GlesVramManagerReset(&state);
    assert(GlesVramManagerSelectTile(&state, GLES_VRAM_TILE_LEFT) ==
           GLES_VRAM_SWITCH_CLEAR);
    assert(GlesVramManagerPersistActive(&state) == GLES_VRAM_TILE_NONE);
    GlesVramManagerMarkActiveDirty(&state);
    assert(GlesVramManagerPersistActive(&state) == GLES_VRAM_TILE_LEFT);
    assert(state.backingValid[GLES_VRAM_TILE_LEFT]);
    assert(!state.efbDirty);
    assert(state.saveCount == 1);
}

static void TestRGB5A3Format(void)
{
    static unsigned char seen[GLES_VRAM_LEFT_WIDTH * GLES_VRAM_HEIGHT];
    int value;
    int widths[2] = {GLES_VRAM_LEFT_WIDTH, GLES_VRAM_RIGHT_WIDTH};
    int widthIndex;

    for (value = 0; value < 0x8000; value++)
    {
        uint16_t gx = GlesVramPsx15ToGXRGB5A3((uint16_t)value);
        assert(gx & 0x8000u);
        assert(GlesVramGXRGB5A3ToPsx15(gx) == (uint16_t)value);
    }

    for (widthIndex = 0; widthIndex < 2; widthIndex++)
    {
        int width = widths[widthIndex];
        int x;
        int y;
        memset(seen, 0, sizeof(seen));
        for (y = 0; y < GLES_VRAM_HEIGHT; y++)
        {
            for (x = 0; x < width; x++)
            {
                size_t offset = GlesVramRGB5A3Offset(width, x, y);
                size_t pixel = offset >> 1;
                assert((offset & 1u) == 0);
                assert(offset + 1 < (size_t)width * GLES_VRAM_HEIGHT * 2);
                assert(!seen[pixel]);
                seen[pixel] = 1;
            }
        }
        for (value = 0; value < width * GLES_VRAM_HEIGHT; value++)
            assert(seen[value]);
    }
}

static uint16_t ReadLittle16(const unsigned short *vram, int x, int y)
{
    const unsigned char *bytes = (const unsigned char *)(vram +
        y * GLES_VRAM_WIDTH + x);
    return (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8);
}

static void WriteLittle16(unsigned short *vram, int x, int y,
                          uint16_t value)
{
    unsigned char *bytes = (unsigned char *)(vram +
        y * GLES_VRAM_WIDTH + x);
    bytes[0] = (unsigned char)value;
    bytes[1] = (unsigned char)(value >> 8);
}

static void TestOverlapSafeWrappedMove(void)
{
    static unsigned short vram[GLES_VRAM_WIDTH * GLES_VRAM_HEIGHT];
    uint16_t scratch[32];
    GlesVramRect source = {1018, 510, 8, 4};
    GlesVramRect destination = {1021, 511, 8, 4};
    uint16_t expected[32];
    int x;
    int y;
    int index = 0;

    memset(vram, 0, sizeof(vram));
    for (y = 0; y < source.height; y++)
    {
        for (x = 0; x < source.width; x++)
        {
            uint16_t value = (uint16_t)(1 + y * source.width + x);
            WriteLittle16(vram,
                          (source.x + x) & (GLES_VRAM_WIDTH - 1),
                          (source.y + y) & (GLES_VRAM_HEIGHT - 1),
                          value);
            expected[index++] = (uint16_t)(value | 0x8000u);
        }
    }

    assert(GlesVramCopyWrapped(vram, &source, &destination, 0x8000u,
                               scratch, sizeof(scratch) / sizeof(scratch[0])));
    index = 0;
    for (y = 0; y < destination.height; y++)
    {
        for (x = 0; x < destination.width; x++)
        {
            assert(ReadLittle16(
                       vram,
                       (destination.x + x) & (GLES_VRAM_WIDTH - 1),
                       (destination.y + y) & (GLES_VRAM_HEIGHT - 1)) ==
                   expected[index++]);
        }
    }
    assert(!GlesVramCopyWrapped(vram, &source, &destination, 0,
                                scratch, 31));
}

static void TestFrontBufferPresentation(void)
{
    GlesVramFrontPresentationState state;
    GlesVramRect leftDisplay = {0, 0, 320, 218};
    GlesVramRect rightDisplay = {320, 0, 320, 218};
    GlesVramRect leftMenu = {142, 157, 178, 56};
    GlesVramRect rightMenu = {462, 157, 178, 56};
    GlesVramRect atlasWrite = {960, 432, 64, 80};
    GlesVramRect selected;

    GlesVramFrontPresentationReset(&state);
    assert(!GlesVramFrontPresentationSelect(
        &state, &leftDisplay, &selected));
    assert(memcmp(&selected, &leftDisplay, sizeof(selected)) == 0);

    /* FF9 updates the currently scanned page, then asks GP1 to scan the
     * other page.  Preserve the visible front-page interval at that switch. */
    GlesVramFrontPresentationMarkWrite(
        &state, &leftMenu, &leftDisplay);
    assert(GlesVramFrontPresentationSelect(
        &state, &rightDisplay, &selected));
    assert(memcmp(&selected, &leftDisplay, sizeof(selected)) == 0);

    GlesVramFrontPresentationMarkWrite(
        &state, &rightMenu, &rightDisplay);
    assert(GlesVramFrontPresentationSelect(
        &state, &leftDisplay, &selected));
    assert(memcmp(&selected, &rightDisplay, sizeof(selected)) == 0);

    /* Texture-atlas writes do not overlap the scanout page and therefore
     * must not delay an ordinary display switch. */
    GlesVramFrontPresentationMarkWrite(
        &state, &atlasWrite, &leftDisplay);
    assert(!GlesVramFrontPresentationSelect(
        &state, &rightDisplay, &selected));
    assert(memcmp(&selected, &rightDisplay, sizeof(selected)) == 0);
}

int main(void)
{
    TestThousandRoundTrips();
    TestBlockSequenceAndWrap();
    TestLazyUploadAndStaleCompletion();
    TestPendingCpuUploadRect();
    TestPersistActive();
    TestRGB5A3Format();
    TestOverlapSafeWrappedMove();
    TestFrontBufferPresentation();
    puts("gpuVramTiling state tests passed");
    return 0;
}
