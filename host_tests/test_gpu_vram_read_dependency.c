#include <stdint.h>
#include <stdio.h>

#include "../GlesGpu/gpuVramReadDependency.h"

static int failures;

static uint32_t NextRandom(uint32_t *state)
{
    *state = *state * 1664525u + 1013904223u;
    return *state;
}

static void expect(int condition, const char *message)
{
    if (!condition)
    {
        printf("FAIL %s\n", message);
        failures++;
    }
}

static GlesGpuVramReadTileState Classify(
    uint64_t cpu, uint64_t materialized, uint64_t efb,
    int full, int partial)
{
    GlesGpuVramReadTileInput input;

    input.cpuWriteSequence = cpu;
    input.materializedSequence = materialized;
    input.efbWriteSequence = efb;
    input.efbHasFullCoverage = full;
    input.efbHasPartialCoverage = partial;
    return GlesGpuVramReadClassifyTile(&input);
}

static void TestOrdering(void)
{
    expect(Classify(10, 0, 9, 1, 0) ==
               GLES_GPU_VRAM_READ_CPU_CURRENT,
           "older EFB cannot overwrite a newer CPU write");
    expect(Classify(10, 0, 10, 1, 0) ==
               GLES_GPU_VRAM_READ_CPU_CURRENT,
           "equal sequence is already CPU-current");
    expect(Classify(10, 0, 11, 1, 0) ==
               GLES_GPU_VRAM_READ_GPU_NEW_FULL,
           "strictly newer full EFB tile requires materialization");
    expect(Classify(5, 12, 11, 1, 0) ==
               GLES_GPU_VRAM_READ_CPU_CURRENT,
           "materialized sequence participates in freshness");
    expect(Classify(5, 12, 13, 1, 0) ==
               GLES_GPU_VRAM_READ_GPU_NEW_FULL,
           "new draw after materialization creates a new dependency");
}

static void TestCoverage(void)
{
    expect(Classify(0, 0, 4, 0, 1) ==
               GLES_GPU_VRAM_READ_GPU_NEW_PARTIAL,
           "partial EFB ownership is reported but not promoted to full");
    expect(Classify(0, 0, 4, 0, 0) ==
               GLES_GPU_VRAM_READ_CPU_CURRENT,
           "sequence without coverage cannot own pixels");
    expect(Classify(0, 0, 4, 1, 1) ==
               GLES_GPU_VRAM_READ_GPU_NEW_FULL,
           "full coverage takes precedence over partial marker");
    expect(GlesGpuVramReadClassifyTile(NULL) ==
               GLES_GPU_VRAM_READ_CPU_CURRENT,
           "null input fails closed to CPU data");
}

static void TestSnapshotGate(void)
{
    expect(GlesGpuVramReadSnapshotMayWrite(7, 0, 8, 1),
           "new full snapshot may update CPU VRAM");
    expect(!GlesGpuVramReadSnapshotMayWrite(8, 0, 8, 1),
           "equal snapshot is not replayed");
    expect(!GlesGpuVramReadSnapshotMayWrite(9, 0, 8, 1),
           "stale snapshot cannot overwrite newer A0 data");
    expect(!GlesGpuVramReadSnapshotMayWrite(1, 9, 8, 1),
           "stale snapshot cannot overwrite materialized data");
    expect(!GlesGpuVramReadSnapshotMayWrite(0, 0, 8, 0),
           "partial snapshot cannot claim a whole tile");
    expect(!GlesGpuVramReadSnapshotMayWrite(0, 0, 0, 1),
           "zero snapshot sequence has no ownership");
}

static void TestRandomOrdering(void)
{
    uint32_t random = 0x4d345f41u;
    int iteration;

    for (iteration = 0; iteration < 20000; iteration++)
    {
        uint64_t cpu = NextRandom(&random) % 1000u;
        uint64_t materialized = NextRandom(&random) % 1000u;
        uint64_t efb = NextRandom(&random) % 1000u;
        uint64_t current = cpu > materialized ? cpu : materialized;
        int full = (int)(NextRandom(&random) & 1u);
        int partial = (int)(NextRandom(&random) & 1u);
        GlesGpuVramReadTileState expected;

        if (efb == 0 || efb <= current || (!full && !partial))
            expected = GLES_GPU_VRAM_READ_CPU_CURRENT;
        else if (full)
            expected = GLES_GPU_VRAM_READ_GPU_NEW_FULL;
        else
            expected = GLES_GPU_VRAM_READ_GPU_NEW_PARTIAL;
        if (Classify(cpu, materialized, efb, full, partial) != expected)
        {
            expect(0, "random tile classification matches sequence model");
            return;
        }
        if (GlesGpuVramReadSnapshotMayWrite(
                cpu, materialized, efb, full) !=
            (full && efb != 0 && efb > current))
        {
            expect(0, "random snapshot gate matches strict ordering");
            return;
        }
    }
    expect(1, "random read dependency ordering remains exact");
}

int main(void)
{
    TestOrdering();
    TestCoverage();
    TestSnapshotGate();
    TestRandomOrdering();

    if (failures == 0)
        printf("PASS gpu_vram_read_dependency\n");
    return failures != 0;
}
