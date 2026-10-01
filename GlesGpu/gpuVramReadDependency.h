#ifndef GPU_VRAM_READ_DEPENDENCY_H
#define GPU_VRAM_READ_DEPENDENCY_H

#include <stddef.h>
#include <stdint.h>

typedef enum GlesGpuVramReadTileStateTag
{
    GLES_GPU_VRAM_READ_CPU_CURRENT = 0,
    GLES_GPU_VRAM_READ_GPU_NEW_FULL,
    GLES_GPU_VRAM_READ_GPU_NEW_PARTIAL
} GlesGpuVramReadTileState;

typedef struct GlesGpuVramReadTileInputTag
{
    uint64_t cpuWriteSequence;
    uint64_t materializedSequence;
    uint64_t efbWriteSequence;
    int efbHasFullCoverage;
    int efbHasPartialCoverage;
} GlesGpuVramReadTileInput;

/*
 * Classifies one 16x16 ownership tile without referring to GX.  CPU and
 * already-materialized sequences are authoritative at equality: only a
 * strictly newer EFB producer creates a read dependency.
 */
static inline uint64_t GlesGpuVramReadCpuSequence(
    uint64_t cpuWriteSequence,
    uint64_t materializedSequence)
{
    return cpuWriteSequence > materializedSequence ?
           cpuWriteSequence : materializedSequence;
}

static inline GlesGpuVramReadTileState GlesGpuVramReadClassifyTile(
    const GlesGpuVramReadTileInput *input)
{
    uint64_t cpuSequence;

    if (input == NULL)
        return GLES_GPU_VRAM_READ_CPU_CURRENT;
    cpuSequence = GlesGpuVramReadCpuSequence(
        input->cpuWriteSequence, input->materializedSequence);
    if (input->efbWriteSequence == 0 ||
        input->efbWriteSequence <= cpuSequence)
        return GLES_GPU_VRAM_READ_CPU_CURRENT;
    if (input->efbHasFullCoverage)
        return GLES_GPU_VRAM_READ_GPU_NEW_FULL;
    if (input->efbHasPartialCoverage)
        return GLES_GPU_VRAM_READ_GPU_NEW_PARTIAL;
    return GLES_GPU_VRAM_READ_CPU_CURRENT;
}

/* A snapshot may update CPU VRAM only when it is strictly newer. */
static inline int GlesGpuVramReadSnapshotMayWrite(
    uint64_t cpuWriteSequence,
    uint64_t materializedSequence,
    uint64_t snapshotSequence,
    int snapshotHasFullCoverage)
{
    return snapshotHasFullCoverage && snapshotSequence != 0 &&
           snapshotSequence > GlesGpuVramReadCpuSequence(
               cpuWriteSequence, materializedSequence);
}

#endif
