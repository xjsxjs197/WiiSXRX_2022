#ifndef GPU_TEXTURE_READ_BARRIER_H
#define GPU_TEXTURE_READ_BARRIER_H

#include <stdint.h>
#include <string.h>

#include "gpuVramRect.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    VRAM_READ_TEXTURE,
    VRAM_READ_PALETTE,
    VRAM_READ_MOVE_SOURCE
} VramReadKind;

enum
{
    T6_SOURCE_WRAP_MAX = 4,
    T6_SOURCE_LINEAR_MAX = 2,
    T6_PALETTE_WRAP_MAX = 4,
    T6_PALETTE_LINEAR_MAX = 2,
    T6_DEP_RECT_CAPACITY = 12,
    T6_VRAM_TILE_X = 64,
    T6_VRAM_TILE_Y = 32,
    T6_VRAM_TILE_SIZE = 16
};

typedef struct VramReadDependency
{
    VramRect rect[T6_DEP_RECT_CAPACITY];
    unsigned int count;
    VramReadKind kind;
} VramReadDependency;

/*
 * The legacy plugin stores GP1(04h) DMA direction in iDataWriteMode, while
 * primLoadImage also uses that mode for the GP0(A0h) pixel payload.  DMA
 * direction alone does not establish a new VRAMWrite descriptor, so payload
 * consumption must additionally be armed by a decoded A0 command.
 */
static inline int T6VramWritePayloadActive(int writeModeIsTransfer,
                                          int descriptorActive)
{
    return writeModeIsTransfer && descriptorActive;
}

/* GP1 DMA direction may leave the legacy mode set before an A0 descriptor
 * exists.  Normalize that parser-only state once; this deliberately does not
 * pretend that a VRAM payload completed (and therefore has no finish/write
 * side effects). */
static inline int T6VramWriteModeNeedsNormalize(int writeModeIsTransfer,
                                                int descriptorActive)
{
    return writeModeIsTransfer && !descriptorActive;
}

/*
 * Cheap conservative prefilter for the production materialize path.  A set
 * bit means that the corresponding 16x16 VRAM tile may contain color newer
 * than psxVuw.  False positives are allowed; false negatives are not.
 */
static inline int T6DependencyIntersectsPotentialTiles(
    const VramReadDependency *dep,
    const uint32_t potential[T6_VRAM_TILE_Y][2])
{
    unsigned int i;

    if (dep == NULL || potential == NULL || dep->count == 0 ||
        dep->count > T6_DEP_RECT_CAPACITY)
        return 1;

    for (i = 0; i < dep->count; i++)
    {
        const VramRect *rect = &dep->rect[i];
        int tx0, tx1, ty0, ty1, ty, tx;

        /* Dependencies produced by the builders are clipped to VRAM.  Keep
         * malformed input fail-closed for callers and host tests. */
        if (rect->x0 < 0 || rect->y0 < 0 ||
            rect->x1 > 1024 || rect->y1 > 512 ||
            VramRectIsEmpty(rect))
            return 1;

        tx0 = rect->x0 >> 4;
        tx1 = (rect->x1 - 1) >> 4;
        ty0 = rect->y0 >> 4;
        ty1 = (rect->y1 - 1) >> 4;
        for (ty = ty0; ty <= ty1; ty++)
            for (tx = tx0; tx <= tx1; tx++)
                if (potential[ty][tx >> 5] &
                    (1u << (tx & 31)))
                    return 1;
    }

    return 0;
}

/* Merge one newly selectable snapshot tile into the conservative summary.
 * Snapshot capture/slot replacement can make a full, newer tile observable
 * without submitting a new EFB draw, so the draw-time summary alone is not
 * sufficient.  This helper deliberately only sets bits: stale positives are
 * safe, while clearing here could create the false-negative which the
 * prefilter contract forbids. */
static inline void T6PotentialMergeSnapshotTile(
    uint32_t potential[T6_VRAM_TILE_Y][2], int tx, int ty,
    int full, uint64_t snapshotSeq,
    uint64_t cpuWriteEpoch, uint64_t materializedColorEpoch)
{
    uint64_t psxColor;

    if (potential == NULL || tx < 0 || tx >= T6_VRAM_TILE_X ||
        ty < 0 || ty >= T6_VRAM_TILE_Y || !full || snapshotSeq == 0)
        return;

    psxColor = cpuWriteEpoch > materializedColorEpoch ?
        cpuWriteEpoch : materializedColorEpoch;
    if (snapshotSeq > psxColor)
        potential[ty][tx >> 5] |= 1u << (tx & 31);
}

typedef enum
{
    VRAM_FRESH_NO_ACTION,
    VRAM_FRESH_MATERIALIZED,
    VRAM_FRESH_UNRESOLVED
} VramFreshResult;

typedef struct VramTileFreshnessInput
{
    uint64_t cpuWriteEpoch;
    uint64_t materializedColorEpoch;
    uint64_t efbSeq;
    /* Highest sequence proven present in the selected snapshot.  The
     * materialize path requires this to be at least the effective EFB
     * sequence; a missing/stale snapshot means capture failed. */
    uint64_t snapshotSeq;
    int efbCoverFull;
    int rgb24;
    int contaminated;
    int mixedMapping;
    int untrackedEfb;
    int hasValidSnapshot;
    /* Baseline delta availability.  When a matching rebuild baseline exists
     * for the snapshot but is stale/partial, the tile must not be written at
     * all: a newer CPU write means even the untouched EFB background cannot
     * be trusted as a full-tile source. */
    int baselinePresentForSnapshot;
    int baselineUsable;
} VramTileFreshnessInput;

typedef struct T6SnapshotCandidate
{
    uint64_t seq;
    int sourcePriority;
    uint64_t captureOrder;
    int qualified;
} T6SnapshotCandidate;

/*
 * Select the unique snapshot for one full tile.  Returns 1-based candidate
 * index (0 when none qualified).  Newer tile sequence wins; equal sequences
 * fall back to source priority and then capture order, mirroring the C0
 * per-pixel tie-break so a tile can never mix two snapshots.
 */
static inline int T6SelectBestSnapshotTile(const T6SnapshotCandidate *cands,
                                           int count, uint64_t *bestSeq)
{
    int best = -1;
    int i;

    if (cands == NULL || count <= 0)
        return 0;

    for (i = 0; i < count; i++)
    {
        const T6SnapshotCandidate *c = &cands[i];

        if (!c->qualified)
            continue;
        if (best < 0 ||
            c->seq > cands[best].seq ||
            (c->seq == cands[best].seq &&
             c->sourcePriority > cands[best].sourcePriority) ||
            (c->seq == cands[best].seq &&
             c->sourcePriority == cands[best].sourcePriority &&
             c->captureOrder > cands[best].captureOrder))
            best = i;
    }

    if (best < 0)
        return 0;
    if (bestSeq != NULL)
        *bestSeq = cands[best].seq;
    return best + 1;
}

/*
 * Materialize plan state.  hazard/resolved/changed are per-tile bitmaps and
 * the counters let the two-phase core prove the whole batch before writing.
 */
typedef struct VramMaterializePlan
{
    unsigned char hazard[T6_VRAM_TILE_Y][T6_VRAM_TILE_X];
    unsigned char resolved[T6_VRAM_TILE_Y][T6_VRAM_TILE_X];
    unsigned char changed[T6_VRAM_TILE_Y][T6_VRAM_TILE_X];
    int hazardCount;
    int resolvedCount;
    int changedCount;
} VramMaterializePlan;

static inline void T6MaterializePlanReset(VramMaterializePlan *plan)
{
    if (plan != NULL)
        memset(plan, 0, sizeof(*plan));
}

static inline void T6MaterializePlanMarkHazard(VramMaterializePlan *plan,
                                               int tx, int ty)
{
    if (plan == NULL || tx < 0 || tx >= T6_VRAM_TILE_X ||
        ty < 0 || ty >= T6_VRAM_TILE_Y)
        return;
    if (!plan->hazard[ty][tx])
        plan->hazardCount++;
    plan->hazard[ty][tx] = 1;
}

static inline void T6MaterializePlanMarkResolved(VramMaterializePlan *plan,
                                                 int tx, int ty)
{
    if (plan == NULL || tx < 0 || tx >= T6_VRAM_TILE_X ||
        ty < 0 || ty >= T6_VRAM_TILE_Y)
        return;
    if (!plan->resolved[ty][tx])
        plan->resolvedCount++;
    plan->resolved[ty][tx] = 1;
}

static inline void T6MaterializePlanMarkChanged(VramMaterializePlan *plan,
                                                int tx, int ty)
{
    if (plan == NULL || tx < 0 || tx >= T6_VRAM_TILE_X ||
        ty < 0 || ty >= T6_VRAM_TILE_Y)
        return;
    if (!plan->changed[ty][tx])
        plan->changedCount++;
    plan->changed[ty][tx] = 1;
}

static inline int T6MaterializePlanAllResolved(const VramMaterializePlan *plan)
{
    return plan != NULL &&
           (plan->hazardCount == 0 || plan->hazardCount == plan->resolvedCount);
}

typedef char T6DepCapacityCheck[
    (T6_SOURCE_WRAP_MAX + T6_SOURCE_LINEAR_MAX +
     T6_PALETTE_WRAP_MAX + T6_PALETTE_LINEAR_MAX ==
     T6_DEP_RECT_CAPACITY) ? 1 : -1];

/*
 * RGB555 low-15-bit freshness sequence of psxVuw for one 16x16 tile.
 * The materialized array is owned by the later T6 stages; the pure helper
 * only combines the two epoch sources.
 */
static inline uint64_t PsxVuwColorTileEpoch(uint64_t cpuWriteEpoch,
                                            uint64_t materializedColorEpoch)
{
    return cpuWriteEpoch > materializedColorEpoch ?
           cpuWriteEpoch : materializedColorEpoch;
}

/*
 * Append one half-open dependency rect with an explicit capacity.  Capacity
 * overflow returns 0 so the caller can fail closed instead of silently
 * dropping a source footprint.
 */
static inline int VramReadDependencyAppendWithCapacity(
    VramReadDependency *dep, const VramRect *rect, unsigned int capacity)
{
    if (dep == NULL || rect == NULL || VramRectIsEmpty(rect) ||
        capacity == 0 || capacity > T6_DEP_RECT_CAPACITY)
        return 0;
    if (dep->count >= capacity)
        return 0;
    dep->rect[dep->count++] = *rect;
    return 1;
}

/*
 * Append one half-open dependency rect using the shared capacity contract.
 */
static inline int VramReadDependencyAppend(VramReadDependency *dep,
                                           const VramRect *rect)
{
    return VramReadDependencyAppendWithCapacity(
        dep, rect, T6_DEP_RECT_CAPACITY);
}

/*
 * Append a whole rect group; on any failure the output is invalidated so a
 * caller can never consume a silently truncated dependency.
 */
static inline int VramReadDependencyAppendRects(
    VramReadDependency *dep, const VramRect *rects, int count,
    unsigned int capacity)
{
    int i;

    if (dep == NULL || rects == NULL || count < 0)
        return 0;
    for (i = 0; i < count; i++)
        if (!VramReadDependencyAppendWithCapacity(dep, &rects[i], capacity))
        {
            dep->count = 0;
            return 0;
        }
    return 1;
}

/*
 * Current linear loader footprint for an arbitrary VRAM source rectangle.
 * Crossing X=1024 advances into the next VRAM row, matching the psxVuw
 * pointer arithmetic used by the texture loaders.
 */
static inline int SourceLinearRects(int x0, int y0, int width, int height,
                                    int vramWidth, int vramHeight,
                                    VramRect out[2])
{
    int x1, overflow, count = 0;

    if (out == NULL || width <= 0 || height <= 0 ||
        vramWidth <= 0 || vramHeight <= 0 ||
        x0 < 0 || y0 < 0 || x0 >= vramWidth || y0 >= vramHeight)
        return 0;

    if (height > vramHeight - y0)
        height = vramHeight - y0;
    if (width > vramWidth)
        width = vramWidth;

    x1 = x0 + width;
    if (x1 <= vramWidth)
    {
        out[0].x0 = x0;
        out[0].y0 = y0;
        out[0].x1 = x1;
        out[0].y1 = y0 + height;
        return VramRectIsEmpty(&out[0]) ? 0 : 1;
    }

    overflow = x1 - vramWidth;

    out[0].x0 = x0;
    out[0].y0 = y0;
    out[0].x1 = vramWidth;
    out[0].y1 = y0 + height;
    count = VramRectIsEmpty(&out[0]) ? 0 : 1;

    if (y0 + 1 < vramHeight)
    {
        out[1].x0 = 0;
        out[1].y0 = y0 + 1;
        out[1].x1 = overflow;
        out[1].y1 = y0 + 1 + height;
        if (out[1].y1 > vramHeight)
            out[1].y1 = vramHeight;
        if (!VramRectIsEmpty(&out[1]))
            count += 1;
    }

    return count;
}

/*
 * Standard (non-interleaved) texture source sub-rectangle in VRAM word
 * coordinates.  Width is derived from the first and last source words so a
 * non-aligned UV range cannot drop edge words.
 */
static inline int StandardTextureSourceWordRect(
    int pageid, int textureMode,
    int uMin, int uMax, int vMin, int vMax,
    VramRect *out)
{
    int xpage = pageid & 15;
    int ypage = pageid >> 4;
    int div;
    int x0, x1, y0, y1;

    if (out == NULL || textureMode < 0 || textureMode > 2 ||
        uMin < 0 || uMax < uMin || uMax > 255 ||
        vMin < 0 || vMax < vMin || vMax > 255)
        return 0;

    div = textureMode == 0 ? 4 : (textureMode == 1 ? 2 : 1);
    x0 = (xpage << 6) + (uMin / div);
    x1 = (xpage << 6) + (uMax / div);
    y0 = (ypage << 8) + vMin;
    y1 = (ypage << 8) + vMax;

    out->x0 = x0;
    out->y0 = y0;
    out->x1 = x1 + 1;
    out->y1 = y1 + 1;
    return 1;
}

static inline int BuildStandardTextureDependencyWithCapacity(
    int pageid, int textureMode, unsigned int clutId,
    int clutYMask, int vramWidth, int vramHeight,
    int uMin, int uMax, int vMin, int vMax,
    int interleaved, unsigned int capacity,
    VramReadDependency *dep)
{
    VramRect pieces[4];
    VramRect sub;
    int n;

    if (dep == NULL || capacity == 0 ||
        capacity > T6_DEP_RECT_CAPACITY ||
        vramWidth != 1024 || vramHeight != 512 ||
        pageid < 0 || pageid > 31 ||
        textureMode < 0 || textureMode > 2 ||
        uMin < 0 || uMax < uMin || uMax > 255 ||
        vMin < 0 || vMax < vMin || vMax > 255 ||
        clutYMask <= 0)
    {
        if (dep != NULL)
            dep->count = 0;
        return 0;
    }

    dep->count = 0;
    dep->kind = VRAM_READ_TEXTURE;

    if (interleaved && textureMode != 2)
    {
        n = TextureWindowSourceRects(pageid, textureMode,
                                     vramWidth, vramHeight, pieces);
        if (n <= 0) { dep->count = 0; return 0; }
        if (!VramReadDependencyAppendRects(dep, pieces, n, capacity))
            return 0;

        n = TextureWindowSourceLinearRects(pageid, textureMode,
                                           vramWidth, vramHeight, pieces);
        if (n <= 0) { dep->count = 0; return 0; }
        if (!VramReadDependencyAppendRects(dep, pieces, n, capacity))
            return 0;
    }
    else
    {
        if (!StandardTextureSourceWordRect(pageid, textureMode,
                                           uMin, uMax, vMin, vMax, &sub))
        {
            dep->count = 0;
            return 0;
        }

        n = SplitWrappedVramRect(sub.x0, sub.y0,
                                 sub.x1 - sub.x0, sub.y1 - sub.y0,
                                 vramWidth, vramHeight, pieces);
        if (n <= 0) { dep->count = 0; return 0; }
        if (!VramReadDependencyAppendRects(dep, pieces, n, capacity))
            return 0;

        n = SourceLinearRects(sub.x0, sub.y0,
                              sub.x1 - sub.x0, sub.y1 - sub.y0,
                              vramWidth, vramHeight, pieces);
        if (n <= 0) { dep->count = 0; return 0; }
        if (!VramReadDependencyAppendRects(dep, pieces, n, capacity))
            return 0;
    }

    if (textureMode != 2)
    {
        n = TextureWindowPaletteRects(clutId, textureMode, clutYMask,
                                      vramWidth, vramHeight, pieces);
        if (n <= 0) { dep->count = 0; return 0; }
        if (!VramReadDependencyAppendRects(dep, pieces, n, capacity))
            return 0;

        n = TextureWindowPaletteLinearRects(clutId, textureMode, clutYMask,
                                            vramWidth, vramHeight, pieces);
        if (n <= 0) { dep->count = 0; return 0; }
        if (!VramReadDependencyAppendRects(dep, pieces, n, capacity))
            return 0;
    }

    return 1;
}

static inline int BuildStandardTextureDependency(
    int pageid, int textureMode, unsigned int clutId,
    int clutYMask, int vramWidth, int vramHeight,
    int uMin, int uMax, int vMin, int vMax,
    int interleaved, VramReadDependency *dep)
{
    return BuildStandardTextureDependencyWithCapacity(
        pageid, textureMode, clutId, clutYMask,
        vramWidth, vramHeight, uMin, uMax, vMin, vMax,
        interleaved, T6_DEP_RECT_CAPACITY, dep);
}

static inline int BuildWindowTextureDependencyWithCapacity(
    int pageid, int textureMode, unsigned int clutId,
    int clutYMask, int vramWidth, int vramHeight,
    unsigned int capacity, VramReadDependency *dep)
{
    VramRect pieces[4];
    int n;

    if (dep == NULL || capacity == 0 ||
        capacity > T6_DEP_RECT_CAPACITY ||
        vramWidth != 1024 || vramHeight != 512 ||
        pageid < 0 || pageid > 31 ||
        textureMode < 0 || textureMode > 2 ||
        clutYMask <= 0)
    {
        if (dep != NULL)
            dep->count = 0;
        return 0;
    }

    dep->count = 0;
    dep->kind = VRAM_READ_TEXTURE;

    n = TextureWindowSourceRects(pageid, textureMode,
                                 vramWidth, vramHeight, pieces);
    if (n <= 0) { dep->count = 0; return 0; }
    if (!VramReadDependencyAppendRects(dep, pieces, n, capacity))
        return 0;

    n = TextureWindowSourceLinearRects(pageid, textureMode,
                                       vramWidth, vramHeight, pieces);
    if (n <= 0) { dep->count = 0; return 0; }
    if (!VramReadDependencyAppendRects(dep, pieces, n, capacity))
        return 0;

    if (textureMode != 2)
    {
        n = TextureWindowPaletteRects(clutId, textureMode, clutYMask,
                                      vramWidth, vramHeight, pieces);
        if (n <= 0) { dep->count = 0; return 0; }
        if (!VramReadDependencyAppendRects(dep, pieces, n, capacity))
            return 0;

        n = TextureWindowPaletteLinearRects(clutId, textureMode, clutYMask,
                                            vramWidth, vramHeight, pieces);
        if (n <= 0) { dep->count = 0; return 0; }
        if (!VramReadDependencyAppendRects(dep, pieces, n, capacity))
            return 0;
    }

    return 1;
}

static inline int BuildWindowTextureDependency(
    int pageid, int textureMode, unsigned int clutId,
    int clutYMask, int vramWidth, int vramHeight,
    VramReadDependency *dep)
{
    return BuildWindowTextureDependencyWithCapacity(
        pageid, textureMode, clutId, clutYMask,
        vramWidth, vramHeight, T6_DEP_RECT_CAPACITY, dep);
}

static inline int BuildMoveSourceDependencyWithCapacity(
    int x, int y, int width, int height,
    int vramWidth, int vramHeight, unsigned int capacity,
    VramReadDependency *dep)
{
    VramRect pieces[4];
    int n;

    if (dep == NULL || capacity == 0 ||
        capacity > T6_DEP_RECT_CAPACITY ||
        vramWidth != 1024 || vramHeight != 512 ||
        x < 0 || y < 0 || x >= 1024 || y >= 512 ||
        width <= 0 || height <= 0)
    {
        if (dep != NULL)
            dep->count = 0;
        return 0;
    }

    dep->count = 0;
    dep->kind = VRAM_READ_MOVE_SOURCE;

    n = SplitWrappedVramRect(x, y, width, height,
                             vramWidth, vramHeight, pieces);
    if (n <= 0) { dep->count = 0; return 0; }
    if (!VramReadDependencyAppendRects(dep, pieces, n, capacity))
        return 0;

    return 1;
}

static inline int BuildMoveSourceDependency(
    int x, int y, int width, int height,
    int vramWidth, int vramHeight,
    VramReadDependency *dep)
{
    return BuildMoveSourceDependencyWithCapacity(
        x, y, width, height, vramWidth, vramHeight,
        T6_DEP_RECT_CAPACITY, dep);
}

/*
 * Shared interleaved materialize decision used by SelectSubTextureS() and by
 * the host harness.  Only a successful materialize that actually changed at
 * least one tile deserves the conservative full-page standard invalidation;
 * a same-color draw still commits the epoch but must keep cache hits.
 */
static inline int T6InterleavedMaterializeShouldInvalidate(
    VramFreshResult result, unsigned int changedTiles, int interleaved)
{
    return result == VRAM_FRESH_MATERIALIZED && changedTiles > 0 &&
           interleaved != 0;
}

/*
 * Packed-position XCHECK used by the standard subtexture cache.  The layout
 * mirrors EXLong.c on a little-endian integer: byte0=y2, byte1=y1, byte2=x2,
 * byte3=x1.  Production entry positions are copied from gl_ux[4..7], so an
 * entry packs vMax|vMin<<8|uMax<<16|uMin<<24.
 */
static inline int T6PackedPosIntersects(unsigned int pos1, unsigned int pos2)
{
    unsigned int y1a = (pos1 >> 8) & 0xFF;
    unsigned int y2a = pos1 & 0xFF;
    unsigned int x1a = (pos1 >> 24) & 0xFF;
    unsigned int x2a = (pos1 >> 16) & 0xFF;
    unsigned int y1b = (pos2 >> 8) & 0xFF;
    unsigned int y2b = pos2 & 0xFF;
    unsigned int x1b = (pos2 >> 24) & 0xFF;
    unsigned int x2b = (pos2 >> 16) & 0xFF;

    return y2a >= y1b && y1a <= y2b && x2a >= x1b && x1a <= x2b;
}

static inline unsigned int T6StandardEntryPackedPos(
    int uMin, int uMax, int vMin, int vMax)
{
    return ((unsigned int)vMax & 0xFF) |
           (((unsigned int)vMin & 0xFF) << 8) |
           (((unsigned int)uMax & 0xFF) << 16) |
           (((unsigned int)uMin & 0xFF) << 24);
}

/*
 * Conservative full-page standard invalidation shared by SelectSubTextureS()
 * and the host harness.  It clears all four SOFFA..SOFFD partitions of one
 * page/mode with a full UV-range packed position, so a page crossing the VRAM
 * right edge invalidates its wrapped UV half as well.
 */
typedef void (*T6StandardPageInvalidateFn)(
    void *user, int pageid, int mode, int partition,
    unsigned int packedPos);

static inline void T6StandardPageInvalidateCore(
    int pageid, int mode, T6StandardPageInvalidateFn callback, void *user)
{
    int p;
    unsigned int packed = 0x00FF00FFu;

    if (callback == NULL || pageid < 0 || pageid > 31 ||
        mode < 0 || mode > 2)
        return;
    for (p = 0; p < 4; p++)
        callback(user, pageid, mode, p, packed);
}

/*
 * Byte-order-independent packed position used by the standard cache sweep.
 * EXLong.c stores y2,y1,x2,x1 as consecutive bytes; the reader bridges real
 * production entries and host T6StandardCacheEntry entries.
 */
typedef struct T6StandardPos
{
    unsigned char y1;
    unsigned char y2;
    unsigned char x1;
    unsigned char x2;
} T6StandardPos;

static inline int T6StandardPosIntersects(const T6StandardPos *pos1,
                                          const T6StandardPos *pos2)
{
    return pos1 != NULL && pos2 != NULL &&
           pos1->y2 >= pos2->y1 && pos1->y1 <= pos2->y2 &&
           pos1->x2 >= pos2->x1 && pos1->x1 <= pos2->x2;
}

/* Layout-equivalent to textureSubCacheEntryS so host tests drive the same
 * sweep with real partition metadata and packed positions. */
typedef struct T6StandardCacheEntry
{
    unsigned int clutId;
    unsigned int packedPos;
    unsigned char posTX;
    unsigned char posTY;
    unsigned char texId;
    unsigned char opaque;
    unsigned int textureType;
} T6StandardCacheEntry;

typedef void (*T6StandardEntryPosReader)(
    void *user, int index, unsigned int *clutIdOut, T6StandardPos *posOut);
typedef void (*T6StandardEntryInvalidateFn)(void *user, int index);

/*
 * One-partition production entry sweep shared by T6InvalidateStandardPageFor-
 * Interleaved() and the host integration harness.  The reader/invalidate pair
 * keeps the sweep independent of pscSubtexStore and EXLong byte order.
 */
static inline void T6InvalidateStandardPartitionSweep(
    int count, unsigned int packedNpos,
    T6StandardEntryPosReader reader, T6StandardEntryInvalidateFn invalidate,
    void *user)
{
    T6StandardPos npos;
    int i;

    if (reader == NULL || invalidate == NULL || count <= 0)
        return;
    npos.y2 = (unsigned char)(packedNpos & 0xFF);
    npos.y1 = (unsigned char)((packedNpos >> 8) & 0xFF);
    npos.x2 = (unsigned char)((packedNpos >> 16) & 0xFF);
    npos.x1 = (unsigned char)((packedNpos >> 24) & 0xFF);

    for (i = 0; i < count; i++)
    {
        unsigned int clutId = 0;
        T6StandardPos pos;

        memset(&pos, 0, sizeof(pos));
        reader(user, i, &clutId, &pos);
        if (clutId != 0 && T6StandardPosIntersects(&pos, &npos))
            invalidate(user, i);
    }
}

/*
 * Shared wiring between the materialize result and the conservative page
 * invalidation.  SelectSubTextureS() and the host harness both call this, so
 * the decision plus the four-partition sweep cannot diverge.
 */
static inline void T6InterleavedStandardBarrier(
    VramFreshResult fresh, unsigned int changedTiles, int interleaved,
    int pageid, int mode,
    T6StandardPageInvalidateFn invalidate, void *user)
{
    if (T6InterleavedMaterializeShouldInvalidate(
            fresh, changedTiles, interleaved))
        T6StandardPageInvalidateCore(pageid, mode, invalidate, user);
}

/*
 * Read-only MoveImage source freshness decision shared by the production
 * shadow and host tests.  MATERIALIZED means the generic path would
 * materialize; UNRESOLVED carries unresolvedReason.  wouldCapture with a
 * valid capturePlanSlot means capture is planned but was not executed.
 */
/*
 * Classification of every non-MATCH exit of the single decision entry
 * T6MoveShadowCompareDecisionEx().  Tile-scan kinds come from
 * T6MoveCompareDetailScanTiles(); the rest are classified in decision
 * order, so the TRB MOVE log reason can never drift from the host
 * decision.
 */
typedef enum
{
    T6_MOVE_BAD_NONE = 0,
    /* Tile-level required/used mismatches (scan order). */
    T6_MOVE_BAD_ABSENT,
    T6_MOVE_BAD_SOURCE_UNKNOWN,
    T6_MOVE_BAD_MIXED,
    T6_MOVE_BAD_MAP,
    T6_MOVE_BAD_SEQ,
    T6_MOVE_BAD_EXTRA,
    T6_MOVE_BAD_USED_COUNT,
    /* Generic before-evidence inconsistent: hazardTiles/requiredTiles/
     * required bitmap disagree or a required tile carries no sequence. */
    T6_MOVE_BAD_BEFORE_EVIDENCE,
    /* Before shadow could not reach a freshness conclusion. */
    T6_MOVE_BAD_BEFORE_UNRESOLVED,
    /* before->result carries a value outside the legal VramFreshResult
     * set (see DecisionEx). */
    T6_MOVE_BAD_RESULT,
    /* Old-path after-evidence relational inconsistency: changed/promoted
     * outside used tiles, mixed without used, stale per-tile fields,
     * pixel/count contradiction or derived succeeded disagreement. */
    T6_MOVE_BAD_CHANGED_OUTSIDE_USED,
    T6_MOVE_BAD_PROMOTED_OUTSIDE_USED,
    T6_MOVE_BAD_AFTER_EVIDENCE,
    /* Top-level comparator exits, ordered like the decision. */
    T6_MOVE_BAD_NOT_EXECUTED,
    T6_MOVE_BAD_NOT_MERGED,
    T6_MOVE_BAD_NOT_SUCCEEDED,
    T6_MOVE_BAD_HASH,
    T6_MOVE_BAD_NOACTION_USED,
    T6_MOVE_BAD_NOACTION_CHANGED,
    T6_MOVE_BAD_NOACTION_PROMOTED,
    T6_MOVE_BAD_NOACTION_INVALIDATED,
    /* NULL argument to the single decision entry. */
    T6_MOVE_BAD_ARGUMENT
} T6MoveBadKind;

typedef struct VramMoveShadowDecision
{
    VramFreshResult result;
    int hazardTiles;
    int captureRequired;
    int wouldCapture;
    int capturePlanSlot;
    int captureBufferReady;
    int unresolvedReason;
    /* Old-path execution and success, filled by the DC2 helper itself. */
    int executed;
    int succeeded;
    /* Content-equivalence evidence: shadow fills map/sequence expectations,
     * the old-path after-decision fills actual changed tiles/pixels/hash. */
    unsigned int changedTiles;
    unsigned int changedPixels;
    unsigned int sourceHash;
    unsigned int requiredMapId;
    uint64_t requiredSeq;
    unsigned int changedTileBitmap[64]; /* 2048 tile bits */
    int evidenceValid;
    /* Per-tile generic expectation. */
    unsigned char requiredTile[T6_VRAM_TILE_Y][T6_VRAM_TILE_X];
    uint64_t requiredTileSeq[T6_VRAM_TILE_Y][T6_VRAM_TILE_X];
    unsigned int requiredTileMapId[T6_VRAM_TILE_Y][T6_VRAM_TILE_X];
    unsigned int requiredTiles;
    /* First tile that made the read-only generic validation fail.  These
     * fields are diagnostic evidence only; they never alter the decision. */
    int unresolvedTileX;
    int unresolvedTileY;
    uint64_t unresolvedBaselineSeq;
    uint64_t unresolvedCpuWriteEpoch;
    uint64_t unresolvedMaterializedEpoch;
    uint64_t unresolvedSnapshotSeq;
    uint64_t unresolvedRequiredSeq;
    uint64_t unresolvedSourceId;
    unsigned int unresolvedMapId;
} VramMoveShadowDecision;

/*
 * Actual execution evidence produced by the old DC2 helper itself.  This is
 * deliberately a different type from the generic decision: result/hazard are
 * generic freshness concepts, while the old path reports what it executed,
 * merged, wrote and promoted.
 */
typedef struct VramMoveOldPathEvidence
{
    int executed;
    int mergeCompleted;
    int succeeded;
    unsigned int changedTiles;
    unsigned int changedPixels;
    unsigned int changedTileBitmap[64];
    unsigned int promotedTileBitmap[64];
    unsigned int promotedTiles;
    unsigned int sourceHash;
    unsigned int requiredMapId;
    uint64_t requiredSeq;
    unsigned int invalidateRuns;
    int evidenceValid;
    /* Per-tile old-path execution records.  For the filtered DC2 merge,
     * usedTile means that every pixel of the full 16x16 source tile was
     * safely resolved (written, proven zero-delta, or protected by a newer
     * materialized epoch), not merely that one pixel entered the write
     * branch. */
    unsigned char usedTile[T6_VRAM_TILE_Y][T6_VRAM_TILE_X];
    unsigned char mixedTile[T6_VRAM_TILE_Y][T6_VRAM_TILE_X];
    uint64_t usedTileSeq[T6_VRAM_TILE_Y][T6_VRAM_TILE_X];
    uint64_t usedTileSourceId[T6_VRAM_TILE_Y][T6_VRAM_TILE_X];
    unsigned int usedTileMapId[T6_VRAM_TILE_Y][T6_VRAM_TILE_X];
    unsigned int usedTiles;
    /* First relational error captured by T6MoveEvidenceFinalize() so the
     * single decision entry can report the precise kind and tile instead of
     * a generic AFTER_EVIDENCE.  NONE when the evidence is consistent; a
     * finalize failure leaves evidenceValid=0 and this field set. */
    T6MoveBadKind evidenceErrorKind;
    int evidenceErrorTx;
    int evidenceErrorTy;
} VramMoveOldPathEvidence;

static inline void T6VramMoveOldPathEvidenceInit(
    VramMoveOldPathEvidence *out)
{
    if (out == NULL)
        return;
    memset(out, 0, sizeof(*out));
}

/* Shared initializer: host helper and production use the same defaults. */
static inline void T6VramMoveShadowDecisionInit(VramMoveShadowDecision *out)
{
    if (out == NULL)
        return;
    memset(out, 0, sizeof(*out));
    out->result = VRAM_FRESH_UNRESOLVED;
    out->capturePlanSlot = -1;
    out->unresolvedTileX = -1;
    out->unresolvedTileY = -1;
}

/* Popcount of the changed-tile bitmap must equal changedTiles. */
static inline int T6MoveEvidenceTileCountMatches(
    const VramMoveOldPathEvidence *ev)
{
    unsigned int changedCount = 0;
    unsigned int promotedCount = 0;
    int i;

    if (ev == NULL)
        return 0;
    for (i = 0; i < 64; i++)
    {
        unsigned int v = ev->changedTileBitmap[i];
        unsigned int p = ev->promotedTileBitmap[i];

        while (v)
        {
            changedCount += v & 1u;
            v >>= 1;
        }
        while (p)
        {
            promotedCount += p & 1u;
            p >>= 1;
        }
    }
    return changedCount == ev->changedTiles &&
           promotedCount == ev->promotedTiles;
}

/*
 * Relational consistency between the old-path evidence arrays: every
 * changed/promoted/mixed tile must be a used tile, unused tiles must not
 * carry stale seq/map/source fields, changed pixels and changed tiles must
 * agree in sign, and the derived succeeded must equal
 * (changedTiles > 0 || promotedTiles > 0).  This is the fail-closed guard
 * for the subset relationships required by the T6-E equivalence evidence;
 * it does not depend on the cached counts alone.
 */
static inline int T6MoveEvidenceBitmapSubsetUsed(
    const VramMoveOldPathEvidence *ev)
{
    int ty, tx;

    if (ev == NULL)
        return 0;
    for (ty = 0; ty < T6_VRAM_TILE_Y; ty++)
        for (tx = 0; tx < T6_VRAM_TILE_X; tx++)
        {
            unsigned int idx = (unsigned int)(ty * T6_VRAM_TILE_X + tx);
            int changed =
                (ev->changedTileBitmap[idx >> 5] >> (idx & 31)) & 1u;
            int promoted =
                (ev->promotedTileBitmap[idx >> 5] >> (idx & 31)) & 1u;

            if (changed && !ev->usedTile[ty][tx])
                return 0;
            if (promoted && !ev->usedTile[ty][tx])
                return 0;
            if (ev->mixedTile[ty][tx] && !ev->usedTile[ty][tx])
                return 0;
            if (!ev->usedTile[ty][tx] &&
                (ev->usedTileSeq[ty][tx] != 0 ||
                 ev->usedTileMapId[ty][tx] != 0 ||
                 ev->usedTileSourceId[ty][tx] != 0))
                return 0;
        }
    if ((ev->changedPixels > 0) != (ev->changedTiles > 0))
        return 0;
    if (ev->succeeded !=
        ((ev->changedTiles > 0 || ev->promotedTiles > 0) ? 1 : 0))
        return 0;
    return 1;
}

/* Per-tile resolved-source collector shared by production and host fixtures.
 * It takes raw observations and detects mixed snapshots itself by comparing
 * sequence, map and stable source identity (snapshot captureOrder). */
static inline void T6MoveRecordUsedTile(
    unsigned char *used, uint64_t *seq, unsigned int *mapId,
    unsigned char *mixed, uint64_t *sourceId, int tileIndex,
    uint64_t tileSeq, unsigned int tileMapId, uint64_t tileSourceId)
{
    if (used == NULL || seq == NULL || mapId == NULL || mixed == NULL ||
        sourceId == NULL ||
        tileIndex < 0 ||
        tileIndex >= T6_VRAM_TILE_X * T6_VRAM_TILE_Y)
        return;
    if (!used[tileIndex])
    {
        used[tileIndex] = 1;
        seq[tileIndex] = tileSeq;
        mapId[tileIndex] = tileMapId;
        sourceId[tileIndex] = tileSourceId;
    }
    else
    {
        if (seq[tileIndex] != tileSeq ||
            mapId[tileIndex] != tileMapId ||
            sourceId[tileIndex] != tileSourceId)
            mixed[tileIndex] = 1;
        if (seq[tileIndex] < tileSeq)
            seq[tileIndex] = tileSeq;
    }
}

/* A generic materialization validates and commits whole 16x16 tiles.  The
 * old filtered path may write only delta pixels, so DIAG evidence can claim
 * equivalence only after every pixel of that tile was safely classified.
 * Zero-delta tiles are therefore resolved without pretending pixels changed. */
static inline int T6MoveRecordResolvedTileIfFull(
    unsigned char *used, uint64_t *seq, unsigned int *mapId,
    unsigned char *mixed, uint64_t *sourceId, int tileIndex,
    unsigned int resolvedPixels, int sourceMixed,
    uint64_t tileSeq, unsigned int tileMapId, uint64_t tileSourceId)
{
    if (resolvedPixels != 16u * 16u || sourceMixed || tileSeq == 0 ||
        tileMapId == 0 || tileSourceId == 0)
        return 0;
    T6MoveRecordUsedTile(used, seq, mapId, mixed, sourceId, tileIndex,
                         tileSeq, tileMapId, tileSourceId);
    return 1;
}

/* Recompute usedTiles from the per-tile bitmap; count must agree. */
static inline int T6MoveEvidenceUsedCountMatches(
    const VramMoveOldPathEvidence *ev)
{
    unsigned int count = 0;
    int ty, tx;

    if (ev == NULL)
        return 0;
    for (ty = 0; ty < T6_VRAM_TILE_Y; ty++)
        for (tx = 0; tx < T6_VRAM_TILE_X; tx++)
            if (ev->usedTile[ty][tx])
                count++;
    return count == ev->usedTiles;
}

/*
 * Find the first relational violation and report its kind plus the tile
 * coordinates (tx/ty = -1 for the derived-sign checks that have no tile).
 * Order: changed outside used, promoted outside used, mixed without used,
 * stale per-tile residue on an unused tile, cached usedTiles disagreeing
 * with the per-tile array, then pixel/count sign and derived succeeded.
 * Returns T6_MOVE_BAD_NONE when the evidence is relationally consistent.
 * This is the shared classifier used by T6MoveEvidenceFinalize() (which
 * stores the result for the decision entry) and by the decision entry
 * itself for hand-built evidence that skipped finalize.
 */
static inline T6MoveBadKind T6MoveEvidenceFirstRelationalError(
    const VramMoveOldPathEvidence *ev, int *outTx, int *outTy)
{
    int ty, tx;

    if (outTx != NULL)
        *outTx = -1;
    if (outTy != NULL)
        *outTy = -1;
    if (ev == NULL)
        return T6_MOVE_BAD_AFTER_EVIDENCE;
    for (ty = 0; ty < T6_VRAM_TILE_Y; ty++)
        for (tx = 0; tx < T6_VRAM_TILE_X; tx++)
        {
            unsigned int idx = (unsigned int)(ty * T6_VRAM_TILE_X + tx);
            int changed =
                (ev->changedTileBitmap[idx >> 5] >> (idx & 31)) & 1u;
            int promoted =
                (ev->promotedTileBitmap[idx >> 5] >> (idx & 31)) & 1u;

            if (changed && !ev->usedTile[ty][tx])
            {
                if (outTx != NULL)
                    *outTx = tx;
                if (outTy != NULL)
                    *outTy = ty;
                return T6_MOVE_BAD_CHANGED_OUTSIDE_USED;
            }
            if (promoted && !ev->usedTile[ty][tx])
            {
                if (outTx != NULL)
                    *outTx = tx;
                if (outTy != NULL)
                    *outTy = ty;
                return T6_MOVE_BAD_PROMOTED_OUTSIDE_USED;
            }
            if (ev->mixedTile[ty][tx] && !ev->usedTile[ty][tx])
            {
                if (outTx != NULL)
                    *outTx = tx;
                if (outTy != NULL)
                    *outTy = ty;
                return T6_MOVE_BAD_AFTER_EVIDENCE;
            }
            if (!ev->usedTile[ty][tx] &&
                (ev->usedTileSeq[ty][tx] != 0 ||
                 ev->usedTileMapId[ty][tx] != 0 ||
                 ev->usedTileSourceId[ty][tx] != 0))
            {
                if (outTx != NULL)
                    *outTx = tx;
                if (outTy != NULL)
                    *outTy = ty;
                return T6_MOVE_BAD_AFTER_EVIDENCE;
            }
        }
    if (!T6MoveEvidenceUsedCountMatches(ev))
    {
        /* Report the first used tile as the anchor when one exists.  The
         * count mismatch is its own kind so the log does not degrade to
         * the generic AFTER_EVIDENCE. */
        for (ty = 0; ty < T6_VRAM_TILE_Y; ty++)
            for (tx = 0; tx < T6_VRAM_TILE_X; tx++)
                if (ev->usedTile[ty][tx])
                {
                    if (outTx != NULL)
                        *outTx = tx;
                    if (outTy != NULL)
                        *outTy = ty;
                    return T6_MOVE_BAD_USED_COUNT;
                }
        return T6_MOVE_BAD_USED_COUNT;
    }
    if ((ev->changedPixels > 0) != (ev->changedTiles > 0))
        return T6_MOVE_BAD_AFTER_EVIDENCE;
    if (ev->succeeded !=
        ((ev->changedTiles > 0 || ev->promotedTiles > 0) ? 1 : 0))
        return T6_MOVE_BAD_AFTER_EVIDENCE;
    return T6_MOVE_BAD_NONE;
}

/*
 * Shared finalize used by production and host fixtures: fills the
 * old-path evidence from the merge-time records and derives succeeded.
 * Returns 0 (evidence invalid) when any array argument is NULL, when tile
 * counts disagree with bitmaps, or when the changed/promoted/mixed/used
 * relational contract is violated.  The first relational error is stored
 * in out->evidenceErrorKind/evidenceErrorTx/evidenceErrorTy so the single
 * decision entry can report the precise reason instead of a generic
 * AFTER_EVIDENCE.
 */
static inline int T6MoveEvidenceFinalize(
    VramMoveOldPathEvidence *out,
    unsigned int changedPixels, unsigned int changedTiles,
    const unsigned int *changedBitmap,
    unsigned int promotedTiles, const unsigned int *promotedBitmap,
    unsigned int invalidateRuns,
    unsigned int sourceHash, unsigned int requiredMapId,
    uint64_t requiredSeq,
    const unsigned char usedTile[T6_VRAM_TILE_Y][T6_VRAM_TILE_X],
    const unsigned char mixedTile[T6_VRAM_TILE_Y][T6_VRAM_TILE_X],
    const uint64_t usedTileSeq[T6_VRAM_TILE_Y][T6_VRAM_TILE_X],
    const uint64_t usedTileSourceId[T6_VRAM_TILE_Y][T6_VRAM_TILE_X],
    const unsigned int usedTileMapId[T6_VRAM_TILE_Y][T6_VRAM_TILE_X])
{
    int ty, tx;
    T6MoveBadKind errKind;
    int errTx, errTy;

    if (out == NULL)
        return 0;
    out->evidenceValid = 0;
    out->evidenceErrorKind = T6_MOVE_BAD_NONE;
    out->evidenceErrorTx = -1;
    out->evidenceErrorTy = -1;
    if (changedBitmap == NULL || promotedBitmap == NULL ||
        usedTile == NULL || mixedTile == NULL ||
        usedTileSeq == NULL || usedTileSourceId == NULL ||
        usedTileMapId == NULL)
        return 0;
    out->mergeCompleted = 1;
    out->changedPixels = changedPixels;
    out->changedTiles = changedTiles;
    memcpy(out->changedTileBitmap, changedBitmap,
           sizeof(out->changedTileBitmap));
    out->promotedTiles = promotedTiles;
    memcpy(out->promotedTileBitmap, promotedBitmap,
           sizeof(out->promotedTileBitmap));
    out->invalidateRuns = invalidateRuns;
    out->sourceHash = sourceHash;
    out->requiredMapId = requiredMapId;
    out->requiredSeq = requiredSeq;
    memcpy(out->usedTile, usedTile, sizeof(out->usedTile));
    memcpy(out->mixedTile, mixedTile, sizeof(out->mixedTile));
    memcpy(out->usedTileSeq, usedTileSeq, sizeof(out->usedTileSeq));
    memcpy(out->usedTileSourceId, usedTileSourceId,
           sizeof(out->usedTileSourceId));
    memcpy(out->usedTileMapId, usedTileMapId,
           sizeof(out->usedTileMapId));
    out->usedTiles = 0;
    for (ty = 0; ty < T6_VRAM_TILE_Y; ty++)
        for (tx = 0; tx < T6_VRAM_TILE_X; tx++)
            if (out->usedTile[ty][tx])
                out->usedTiles++;
    out->succeeded = (changedTiles > 0 || promotedTiles > 0) ? 1 : 0;
    if (!T6MoveEvidenceTileCountMatches(out))
    {
        out->evidenceErrorKind = T6_MOVE_BAD_AFTER_EVIDENCE;
        return 0;
    }
    errKind = T6MoveEvidenceFirstRelationalError(out, &errTx, &errTy);
    if (errKind != T6_MOVE_BAD_NONE)
    {
        out->evidenceErrorKind = errKind;
        out->evidenceErrorTx = errTx;
        out->evidenceErrorTy = errTy;
        return 0;
    }
    out->evidenceValid = 1;
    return 1;
}


/*
 * Complete old-path evidence consistency: cached counts agree with the
 * per-tile arrays and the changed/promoted/mixed/used subset contract
 * holds.  The comparator must fail closed on any violation instead of
 * trusting instrumentation that could under- or over-count.
 */
static inline int T6MoveEvidenceRelationalConsistent(
    const VramMoveOldPathEvidence *ev)
{
    if (ev == NULL)
        return 0;
    if (!T6MoveEvidenceUsedCountMatches(ev))
        return 0;
    if (!T6MoveEvidenceTileCountMatches(ev))
        return 0;
    return T6MoveEvidenceBitmapSubsetUsed(ev);
}

/*
 * Generic before-evidence consistency: the required tile bitmap popcount
 * must equal requiredTiles and hazardTiles, a MATERIALIZED decision needs
 * at least one required tile, a NO_ACTION decision must have none, and
 * every required tile must carry a non-zero required sequence.  An
 * inconsistent generic expectation cannot be trusted, so the comparator
 * must return INCONCLUSIVE (BAD_BEFORE_EVIDENCE).
 */
static inline int T6MoveRequiredEvidenceConsistent(
    const VramMoveShadowDecision *before)
{
    unsigned int count = 0;
    int ty, tx;

    if (before == NULL)
        return 0;
    for (ty = 0; ty < T6_VRAM_TILE_Y; ty++)
        for (tx = 0; tx < T6_VRAM_TILE_X; tx++)
            if (before->requiredTile[ty][tx])
            {
                count++;
                /* A required tile must carry a provable mapping identity:
                 * map id 0 means no mapping could be pinned, so it can
                 * never serve as content-equivalence evidence (an actual
                 * map of 0 would compare equal and fake a MATCH). */
                if (before->requiredTileSeq[ty][tx] == 0 ||
                    before->requiredTileMapId[ty][tx] == 0)
                    return 0;
            }
            else if (before->requiredTileSeq[ty][tx] != 0 ||
                     before->requiredTileMapId[ty][tx] != 0)
            {
                /* Mirror of the after-evidence stale-field check: a tile
                 * that is not required must not carry per-tile
                 * expectations that would be silently ignored. */
                return 0;
            }
    if (count != before->requiredTiles)
        return 0;
    if ((unsigned int)before->hazardTiles != count)
        return 0;
    if (before->result == VRAM_FRESH_MATERIALIZED && count == 0)
        return 0;
    if (before->result == VRAM_FRESH_NO_ACTION && count != 0)
        return 0;
    /* Aggregate requiredMapId/requiredSeq are summary fields written by
     * the production helper purely for the TRB MOVE log; the decision
     * itself only consumes the per-tile arrays above, so they are not
     * re-validated here. */
    return 1;
}

/* Shared merge-time record collectors.  Production passes the DIAG arrays
 * filled by MergeReadbackToPsxVuwFiltered(); host fixtures use the same
 * helpers with asymmetric (tx != ty) coordinates. */
static inline uint64_t T6MoveEvidenceMaxUsedSeq(
    const uint64_t seq[T6_VRAM_TILE_Y][T6_VRAM_TILE_X],
    const unsigned char used[T6_VRAM_TILE_Y][T6_VRAM_TILE_X])
{
    uint64_t maxSeq = 0;
    int ty, tx;

    if (seq == NULL || used == NULL)
        return 0;
    for (ty = 0; ty < T6_VRAM_TILE_Y; ty++)
        for (tx = 0; tx < T6_VRAM_TILE_X; tx++)
            if (used[ty][tx] && seq[ty][tx] > maxSeq)
                maxSeq = seq[ty][tx];
    return maxSeq;
}

static inline unsigned int T6MoveEvidenceUsedMapId(
    const unsigned int mapId[T6_VRAM_TILE_Y][T6_VRAM_TILE_X],
    const unsigned char used[T6_VRAM_TILE_Y][T6_VRAM_TILE_X])
{
    unsigned int id = 0;
    int ty, tx;

    if (mapId == NULL || used == NULL)
        return 0;
    for (ty = 0; ty < T6_VRAM_TILE_Y; ty++)
        for (tx = 0; tx < T6_VRAM_TILE_X; tx++)
            if (used[ty][tx])
            {
                if (id == 0)
                    id = mapId[ty][tx];
                else if (mapId[ty][tx] != id)
                    return 0;
            }
    return id;
}

typedef struct
{
    T6MoveBadKind kind;
    int tx, ty;
    uint64_t requiredSeq, actualSeq;
    unsigned int requiredMapId, actualMapId;
    uint64_t actualSourceId;
    unsigned int missingTiles, mixedTiles, extraTiles;
} T6MoveCompareDetail;

/*
 * Shared per-tile mismatch scan used by the decision evaluator and by TRB
 * MOVE logs, so real-device diagnosis can never drift from the host
 * decision.  This only classifies tile-level required/used problems;
 * top-level evidence problems are classified by the evaluator itself.
 */
static inline void T6MoveCompareDetailScanTiles(
    const VramMoveShadowDecision *before,
    const VramMoveOldPathEvidence *after,
    T6MoveCompareDetail *out)
{
    unsigned int missing = 0, mixedCount = 0, extra = 0;
    T6MoveBadKind firstKind = T6_MOVE_BAD_NONE;
    int firstTx = -1, firstTy = -1;
    uint64_t firstReqSeq = 0, firstActSeq = 0, firstSourceId = 0;
    unsigned int firstReqMap = 0, firstActMap = 0;
    int ty, tx;

    if (out == NULL)
        return;
    memset(out, 0, sizeof(*out));
    out->kind = T6_MOVE_BAD_NONE;
    out->tx = -1;
    out->ty = -1;
    if (before == NULL || after == NULL)
        return;

    /* Pass 1: independent totals.  missing counts required tiles that were
     * never fully resolved; mixedTiles counts every resolved tile carrying a mixed
     * snapshot (whether or not it is required or extra); extra counts used
     * tiles outside the required set.  These are independent of the
     * first-bad priority so the log's mixed= is a true problem-size
     * figure.  They are computed before the used-count gate so a count
     * mismatch still reports the totals (89.1). */
    for (ty = 0; ty < T6_VRAM_TILE_Y; ty++)
        for (tx = 0; tx < T6_VRAM_TILE_X; tx++)
        {
            if (before->requiredTile[ty][tx] &&
                !after->usedTile[ty][tx])
                missing++;
            if (after->usedTile[ty][tx] &&
                after->mixedTile[ty][tx])
                mixedCount++;
            if (after->usedTile[ty][tx] &&
                !before->requiredTile[ty][tx])
                extra++;
        }

    /* Cached usedTiles must agree with the per-tile bitmap.  A mismatch
     * decides kind/tx/ty only; the independent totals above stay intact. */
    if (!T6MoveEvidenceUsedCountMatches(after))
    {
        out->missingTiles = missing;
        out->mixedTiles = mixedCount;
        out->extraTiles = extra;
        out->kind = T6_MOVE_BAD_USED_COUNT;
        for (ty = 0; ty < T6_VRAM_TILE_Y; ty++)
            for (tx = 0; tx < T6_VRAM_TILE_X; tx++)
                if (after->usedTile[ty][tx])
                {
                    out->tx = tx;
                    out->ty = ty;
                    return;
                }
        return;
    }

    /* Pass 2: first-bad priority and the detailed fields of that tile. */
    for (ty = 0; ty < T6_VRAM_TILE_Y; ty++)
        for (tx = 0; tx < T6_VRAM_TILE_X; tx++)
            if (before->requiredTile[ty][tx])
            {
                T6MoveBadKind kind = T6_MOVE_BAD_NONE;

                if (!after->usedTile[ty][tx])
                    kind = T6_MOVE_BAD_ABSENT;
                else if (after->usedTileSourceId[ty][tx] == 0)
                    kind = T6_MOVE_BAD_SOURCE_UNKNOWN;
                else if (after->mixedTile[ty][tx])
                    kind = T6_MOVE_BAD_MIXED;
                else if (after->usedTileMapId[ty][tx] !=
                         before->requiredTileMapId[ty][tx])
                    kind = T6_MOVE_BAD_MAP;
                else if (after->usedTileSeq[ty][tx] <
                         before->requiredTileSeq[ty][tx])
                    kind = T6_MOVE_BAD_SEQ;
                if (firstKind == T6_MOVE_BAD_NONE &&
                    kind != T6_MOVE_BAD_NONE)
                {
                    firstKind = kind;
                    firstTx = tx;
                    firstTy = ty;
                    firstReqSeq = before->requiredTileSeq[ty][tx];
                    firstReqMap = before->requiredTileMapId[ty][tx];
                    if (after->usedTile[ty][tx])
                    {
                        firstActSeq = after->usedTileSeq[ty][tx];
                        firstActMap = after->usedTileMapId[ty][tx];
                        firstSourceId = after->usedTileSourceId[ty][tx];
                    }
                }
            }

    for (ty = 0; ty < T6_VRAM_TILE_Y; ty++)
        for (tx = 0; tx < T6_VRAM_TILE_X; tx++)
            if (after->usedTile[ty][tx] &&
                !before->requiredTile[ty][tx])
            {
                if (firstKind == T6_MOVE_BAD_NONE)
                {
                    firstKind = T6_MOVE_BAD_EXTRA;
                    firstTx = tx;
                    firstTy = ty;
                    firstActSeq = after->usedTileSeq[ty][tx];
                    firstActMap = after->usedTileMapId[ty][tx];
                    firstSourceId = after->usedTileSourceId[ty][tx];
                }
            }

    out->missingTiles = missing;
    out->mixedTiles = mixedCount;
    out->extraTiles = extra;
    out->kind = firstKind;
    out->tx = firstTx;
    out->ty = firstTy;
    out->requiredSeq = firstReqSeq;
    out->actualSeq = firstActSeq;
    out->requiredMapId = firstReqMap;
    out->actualMapId = firstActMap;
    out->actualSourceId = firstSourceId;
}

typedef enum
{
    T6_MOVE_COMPARE_MATCH = 0,
    T6_MOVE_COMPARE_MISMATCH,
    T6_MOVE_COMPARE_INCONCLUSIVE
} T6MoveShadowCompare;

/*
 * Single complete decision entry shared by production and host tests.  It
 * classifies every non-MATCH exit into a T6MoveBadKind inside detailOut,
 * and it is the only place that decides MATCH/MISMATCH/INCONCLUSIVE, so the
 * real-device log reason can never drift from the host decision.  The
 * tile-level scan is delegated to T6MoveCompareDetailScanTiles(); evidence
 * and ordering problems are classified here in decision order.
 */
static inline T6MoveShadowCompare T6MoveShadowCompareDecisionEx(
    const VramMoveShadowDecision *before,
    const VramMoveOldPathEvidence *after,
    T6MoveCompareDetail *detailOut)
{
    T6MoveCompareDetail local;
    T6MoveCompareDetail *d = detailOut != NULL ? detailOut : &local;

    memset(d, 0, sizeof(*d));
    d->kind = T6_MOVE_BAD_NONE;
    d->tx = -1;
    d->ty = -1;

    if (before == NULL || after == NULL)
    {
        d->kind = T6_MOVE_BAD_ARGUMENT;
        return T6_MOVE_COMPARE_INCONCLUSIVE;
    }
    /* before->result must be one of the legal freshness outcomes; an
     * out-of-range value (e.g. a corrupted enum) is its own badKind so no
     * non-MATCH exit is ever left with BAD_NONE. */
    if (before->result != VRAM_FRESH_UNRESOLVED &&
        before->result != VRAM_FRESH_NO_ACTION &&
        before->result != VRAM_FRESH_MATERIALIZED)
    {
        d->kind = T6_MOVE_BAD_RESULT;
        return T6_MOVE_COMPARE_INCONCLUSIVE;
    }
    if (before->result == VRAM_FRESH_UNRESOLVED)
    {
        /* Unreachable freshness: the unresolvedReason field carries the
         * specific cause; the badKind names the gate itself so no
         * non-MATCH exit is left unclassified. */
        d->kind = T6_MOVE_BAD_BEFORE_UNRESOLVED;
        return T6_MOVE_COMPARE_INCONCLUSIVE;
    }
    if (!T6MoveRequiredEvidenceConsistent(before))
    {
        d->kind = T6_MOVE_BAD_BEFORE_EVIDENCE;
        return T6_MOVE_COMPARE_INCONCLUSIVE;
    }
    /* executed/mergeCompleted are checked before the generic evidenceValid
     * fallback so a production early-return (executed=1, mergeCompleted=0,
     * evidenceValid=0) is reported as NOT_MERGED instead of a generic
     * AFTER_EVIDENCE. */
    if (!after->executed)
    {
        d->kind = T6_MOVE_BAD_NOT_EXECUTED;
        return T6_MOVE_COMPARE_MISMATCH;
    }
    if (!after->mergeCompleted)
    {
        d->kind = T6_MOVE_BAD_NOT_MERGED;
        return T6_MOVE_COMPARE_MISMATCH;
    }
    /* Finalize-captured relational error: precise kind plus tile. */
    if (after->evidenceErrorKind != T6_MOVE_BAD_NONE)
    {
        d->kind = after->evidenceErrorKind;
        d->tx = after->evidenceErrorTx;
        d->ty = after->evidenceErrorTy;
        return T6_MOVE_COMPARE_INCONCLUSIVE;
    }
    if (!T6MoveEvidenceRelationalConsistent(after))
    {
        /* Hand-built evidence that skipped finalize: classify live so the
         * log still names the precise violation and tile.  A used-count
         * mismatch additionally carries the independent scan totals, so it
         * is routed through ScanTiles (which reports USED_COUNT with
         * missing/mixed/extra intact instead of zeroing them). */
        int tx = -1, ty = -1;
        T6MoveBadKind k =
            T6MoveEvidenceFirstRelationalError(after, &tx, &ty);

        if (k == T6_MOVE_BAD_USED_COUNT)
        {
            T6MoveCompareDetailScanTiles(before, after, d);
            return T6_MOVE_COMPARE_INCONCLUSIVE;
        }
        if (k == T6_MOVE_BAD_NONE)
            k = T6_MOVE_BAD_AFTER_EVIDENCE;
        d->kind = k;
        d->tx = tx;
        d->ty = ty;
        return T6_MOVE_COMPARE_INCONCLUSIVE;
    }
    if (!after->evidenceValid)
    {
        d->kind = T6_MOVE_BAD_AFTER_EVIDENCE;
        return T6_MOVE_COMPARE_INCONCLUSIVE;
    }
    if (before->result == VRAM_FRESH_NO_ACTION)
    {
        /* CPU-newer/no-op: the old path must prove it wrote nothing, left
         * the source hash unchanged and produced no epoch/cache side
         * effects.  changed/promoted are checked before usedTiles so the
         * specific side-effect kinds stay reachable (a changed or promoted
         * tile is by contract a used tile, so usedTiles>0 would otherwise
         * shadow them). */
        if (after->changedTiles != 0 || after->changedPixels != 0)
        {
            d->kind = T6_MOVE_BAD_NOACTION_CHANGED;
            return T6_MOVE_COMPARE_MISMATCH;
        }
        if (after->promotedTiles != 0)
        {
            d->kind = T6_MOVE_BAD_NOACTION_PROMOTED;
            return T6_MOVE_COMPARE_MISMATCH;
        }
        if (after->usedTiles != 0)
        {
            d->kind = T6_MOVE_BAD_NOACTION_USED;
            return T6_MOVE_COMPARE_MISMATCH;
        }
        if (after->invalidateRuns != 0)
        {
            d->kind = T6_MOVE_BAD_NOACTION_INVALIDATED;
            return T6_MOVE_COMPARE_MISMATCH;
        }
        if (after->sourceHash != before->sourceHash)
        {
            d->kind = T6_MOVE_BAD_HASH;
            return T6_MOVE_COMPARE_MISMATCH;
        }
        return T6_MOVE_COMPARE_MATCH;
    }
    if (before->result == VRAM_FRESH_MATERIALIZED)
    {
        T6MoveCompareDetailScanTiles(before, after, d);
        if (d->kind != T6_MOVE_BAD_NONE)
        {
            if (d->kind == T6_MOVE_BAD_SOURCE_UNKNOWN ||
                d->kind == T6_MOVE_BAD_MIXED)
                return T6_MOVE_COMPARE_INCONCLUSIVE;
            return T6_MOVE_COMPARE_MISMATCH;
        }
        if (!after->succeeded)
        {
            d->kind = T6_MOVE_BAD_NOT_SUCCEEDED;
            return T6_MOVE_COMPARE_MISMATCH;
        }
        if ((after->changedTiles > 0 || after->changedPixels > 0) &&
            after->sourceHash == before->sourceHash)
        {
            d->kind = T6_MOVE_BAD_HASH;
            return T6_MOVE_COMPARE_MISMATCH;
        }
        return T6_MOVE_COMPARE_MATCH;
    }
    /* Defensive: every legal result value is handled above, so this exit
     * is unreachable; it still carries a kind to keep the no-BAD_NONE
     * contract. */
    d->kind = T6_MOVE_BAD_RESULT;
    return T6_MOVE_COMPARE_INCONCLUSIVE;
}

/*
 * Host-testable equivalence comparator: generic expectation (before) versus
 * old-path execution evidence (after).  Evidence is produced by the DC2
 * helper itself, not by a second generic decision.  Thin wrapper around
 * T6MoveShadowCompareDecisionEx() so callers that only need the verdict
 * share exactly the same decision path as the diagnostic logger.
 */
static inline T6MoveShadowCompare T6MoveShadowCompareDecision(
    const VramMoveShadowDecision *before,
    const VramMoveOldPathEvidence *after)
{
    return T6MoveShadowCompareDecisionEx(before, after, NULL);
}

/* Explicit output reset used by every changedTilesOut early-return path. */
static inline void T6ResetChangedTilesOutput(unsigned int *out)
{
    if (out != NULL)
        *out = 0;
}

/* T6-E-2 wrapper 前置 fail-closed 检查（EnsureVramMoveSourceFresh 契约 1）：
 * 1024-line backing（iGPUHeight != 512）、非法 rect（x/y 越界或 w/h <= 0）、
 * readback disabled 或 reentrant（workspace/barrier depth 非零）一律拒绝且零
 * 写入。wrap 是合法输入：X/Y/X+Y wrap dependency 由 BuildMoveSourceDependency
 * 分段处理，这里只验证起点与正值尺寸。 */
static inline int T6MoveSourceFreshnessPrecheck(
    int x, int y, int w, int h, int vramHeight,
    int readbackEnabled, int workspaceDepth, int barrierDepth)
{
    if (!readbackEnabled || w <= 0 || h <= 0 ||
        x < 0 || y < 0 || x >= 1024 || y >= 512 ||
        vramHeight != 512 ||
        workspaceDepth != 0 || barrierDepth != 0)
        return 0;
    return 1;
}

/* P2-F3: generic 成功后 post 决策是否构成 takeover regression（每个
 * MoveImage 最多计一次）。generic 未成功（UNRESOLVED）是预期的
 * fail-closed 状态，不计入 success-post regression，由 takeUnresolved
 * 统计；generic 成功但 post 不是 clean
 * NO_ACTION（result 非 NO_ACTION、reason 非 NONE 或 hazard 非零）才计。
 * 注：T6_REASON_NONE 定义在 T6Reason 枚举区（本文件后部），此函数本体在
 * 枚举之后定义。 */

/* DIAG takeover evidence workspace。T6-E-3 删除 DC2 old-path fallback 后只
 * 需要 generic 前/后的两份只读 decision（约 53 KiB）；仍放在 MEM2 并用
 * busy guard 保证不可重入，避免把大对象放回 GPU command 栈。 */
typedef struct T6MoveTakeDiagWorkspace
{
    VramMoveShadowDecision before;
    VramMoveShadowDecision post;
    int busy;
} T6MoveTakeDiagWorkspace;

/* P2-F4: TAKE timing 用 totals 差分（无副作用），单次 delta 饱和到 UINT_MAX
 * 防 uint64->uint32 截断。early-return 路径（totals 未变）delta 自然为 0。 */
static inline unsigned int T6MoveTimingDeltaUs(
    uint64_t before, uint64_t after)
{
    uint64_t d = after - before;

    return d > 0xFFFFFFFFu ? 0xFFFFFFFFu : (unsigned int)d;
}

/* P1-F1: production-equivalent MoveImage CPU copy core。与 gpuPrim.c 既存
 * copy formula 逐字节等价的三分支：
 * - wrap 分段（source/destination 越界）：每像素经 &0x3ff / &(vramHeight-1)
 *   取模，不应用 setMask（与既存 MoveImageWrapped 一致）；
 * - unaligned（(sx|x0|x1)&1）：16-bit 拷贝并 OR setMask16；
 * - aligned：32-bit 拷贝并 OR setMask32。
 * production 在 CPU source read 前完成 generic freshness 后调用本 core；
 * host 测试用静态 VRAM 模拟 source/destination，逐像素断言 background/mask。 */
static inline void T6MoveCopyRect(
    unsigned short *vram, int vramHeight,
    int imageX0, int imageY0, int imageX1, int imageY1,
    int imageSX, int imageSY,
    unsigned long setMask32, unsigned short setMask16)
{
    int i, j;

    if ((imageY0 + imageSY) > vramHeight ||
        (imageX0 + imageSX) > 1024 ||
        (imageY1 + imageSY) > vramHeight ||
        (imageX1 + imageSX) > 1024)
    {
        for (j = 0; j < imageSY; j++)
            for (i = 0; i < imageSX; i++)
                vram[(1024 * ((imageY1 + j) & (vramHeight - 1))) +
                     ((imageX1 + i) & 0x3ff)] =
                    vram[(1024 * ((imageY0 + j) & (vramHeight - 1))) +
                         ((imageX0 + i) & 0x3ff)];
        return;
    }
    if ((imageSX | imageX0 | imageX1) & 1)
    {
        unsigned short *SRCPtr =
            vram + 1024 * imageY0 + imageX0;
        unsigned short *DSTPtr =
            vram + 1024 * imageY1 + imageX1;
        unsigned short LineOffset = 1024 - imageSX;

        for (j = 0; j < imageSY; j++)
        {
            for (i = 0; i < imageSX; i++)
                *DSTPtr++ = (*SRCPtr++) | setMask16;
            SRCPtr += LineOffset;
            DSTPtr += LineOffset;
        }
    }
    else
    {
        unsigned int *SRCPtr =
            (unsigned int *)(vram + 1024 * imageY0 + imageX0);
        unsigned int *DSTPtr =
            (unsigned int *)(vram + 1024 * imageY1 + imageX1);
        unsigned short LineOffset = 512 - (imageSX >> 1);
        int dx = imageSX >> 1;

        for (j = 0; j < imageSY; j++)
        {
            for (i = 0; i < dx; i++)
                *DSTPtr++ = (*SRCPtr++) | (unsigned int)setMask32;
            SRCPtr += LineOffset;
            DSTPtr += LineOffset;
        }
    }
}

/*
 * Shared early-return entry used by both production read-fresh entrances.
 * Unlike a mutable gate struct it takes explicit stage booleans, so callers
 * can never read an uninitialized field.  It resets changedTilesOut first,
 * then fails closed for disabled readback, invalid dependency, outer barrier
 * reentrancy and inner workspace reentrancy.
 */
static inline VramFreshResult T6ReadFreshEntry(
    int readbackEnabled, int dependencyValid,
    int outerWorkspaceAvailable, int innerWorkspaceAvailable,
    unsigned int *changedTilesOut)
{
    if (changedTilesOut != NULL)
        *changedTilesOut = 0;
    if (!readbackEnabled || !dependencyValid ||
        !outerWorkspaceAvailable || !innerWorkspaceAvailable)
        return VRAM_FRESH_UNRESOLVED;
    return VRAM_FRESH_NO_ACTION;
}

/*
 * Pure per-tile freshness decision.  This is the T6-A observer contract;
 * materialize conditions beyond the tile state (baseline, capture) are
 * introduced by the later stages.
 */
static inline VramFreshResult EvaluateVramTileFreshness(
    const VramTileFreshnessInput *in)
{
    uint64_t psxColorSeq;

    if (in == NULL)
        return VRAM_FRESH_UNRESOLVED;

    psxColorSeq = PsxVuwColorTileEpoch(in->cpuWriteEpoch,
                                       in->materializedColorEpoch);

    if (in->efbSeq == 0 || in->efbSeq <= psxColorSeq)
        return VRAM_FRESH_NO_ACTION;

    if (!in->efbCoverFull || in->rgb24 || in->contaminated ||
        in->mixedMapping || in->untrackedEfb || !in->hasValidSnapshot)
        return VRAM_FRESH_UNRESOLVED;

    return VRAM_FRESH_MATERIALIZED;
}

/*
 * Materialize-core freshness decision.  In addition to the T6-A tile
 * contract, the selected snapshot must actually cover the effective EFB
 * sequence, and a present-but-unusable baseline makes the tile unresolved
 * instead of falling back to a full-tile overwrite.
 */
static inline VramFreshResult EvaluateVramMaterializeTile(
    const VramTileFreshnessInput *in)
{
    VramFreshResult base;

    if (in == NULL)
        return VRAM_FRESH_UNRESOLVED;

    base = EvaluateVramTileFreshness(in);
    if (base != VRAM_FRESH_MATERIALIZED)
        return base;

    if (in->snapshotSeq == 0 || in->snapshotSeq < in->efbSeq)
        return VRAM_FRESH_UNRESOLVED;

    if (in->baselinePresentForSnapshot && !in->baselineUsable)
        return VRAM_FRESH_UNRESOLVED;

    return VRAM_FRESH_MATERIALIZED;
}

/*
 * PS1 writeback keeps the old mask bit; RGB5A3 bit15/alpha never becomes the
 * PS1 mask.  This is the only pixel formula used by materialize.
 */
static inline uint16_t T6MergePixelColor(uint16_t oldPsx, uint16_t gxColor)
{
    return (uint16_t)((gxColor & 0x7FFFu) | (oldPsx & 0x8000u));
}

/*
 * Precomputed byte offsets for sampling one 16x16 PS1 VRAM tile from a GX
 * RGB5A3 texture.  The scalar snapshot reader normally performs two mapping
 * divisions per pixel.  Materialize compares two snapshots which share the
 * same sampling map, so computing the 16 X and 16 Y components once removes
 * hundreds of divisions without changing the selected samples.
 */
typedef struct T6RGB5A3TileSamplePlan
{
    unsigned int xByteOffset[T6_VRAM_TILE_SIZE];
    unsigned int yByteOffset[T6_VRAM_TILE_SIZE];
} T6RGB5A3TileSamplePlan;

static inline int T6BuildRGB5A3TileSamplePlan(
    T6RGB5A3TileSamplePlan *plan, int tileX, int tileY,
    int vramX, int vramY, int vramW, int vramH,
    int viewportX, int viewportY, int viewportW, int viewportH,
    int texWidth, int texHeight)
{
    int blocksPerRow;
    int i;

    if (plan == NULL || tileX < 0 || tileX >= T6_VRAM_TILE_X ||
        tileY < 0 || tileY >= T6_VRAM_TILE_Y ||
        vramW <= 0 || vramH <= 0 || viewportW <= 0 || viewportH <= 0 ||
        texWidth <= 0 || texHeight <= 0 || (texWidth & 3) != 0)
        return 0;

    blocksPerRow = texWidth >> 2;
    for (i = 0; i < T6_VRAM_TILE_SIZE; i++)
    {
        int px = (tileX << 4) + i;
        int nx = (int)((unsigned int)(px - vramX) & 1023u);
        int sx;

        if (nx >= vramW)
            return 0;
        sx = viewportX + (nx * viewportW) / vramW;
        if (sx < 0 || sx >= texWidth)
            return 0;
        plan->xByteOffset[i] =
            (unsigned int)((sx >> 2) << 5) +
            (unsigned int)((sx & 3) << 1);
    }

    for (i = 0; i < T6_VRAM_TILE_SIZE; i++)
    {
        int py = (tileY << 4) + i;
        int ny = (int)((unsigned int)(py - vramY) & 511u);
        int sy;

        if (ny >= vramH)
            return 0;
        sy = viewportY + (ny * viewportH) / vramH;
        if (sy < 0 || sy >= texHeight)
            return 0;
        plan->yByteOffset[i] =
            (unsigned int)((sy >> 2) * blocksPerRow << 5) +
            (unsigned int)((sy & 3) << 3);
    }

    return 1;
}

static inline uint16_t T6ReadRGB5A3TileSample(
    const unsigned char *pixels, const T6RGB5A3TileSamplePlan *plan,
    int localX, int localY)
{
    const unsigned char *p =
        pixels + plan->xByteOffset[localX] + plan->yByteOffset[localY];

    return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

/*
 * C0/T6 cross guard: a C0 snapshot whose tile sequence is older than the
 * materialized color epoch must not overwrite the low 15 bits.  Equal
 * sequences are allowed per the design review note.
 */
static inline int T6C0ColorCommitAllowed(uint64_t snapshotSeq,
                                         uint64_t materializedColorEpoch)
{
    return snapshotSeq >= materializedColorEpoch;
}

/* A whole tile may advance its materialized color epoch after all 256 pixels
 * were safely resolved from one snapshot.  Pixels retained because the
 * snapshot equals its baseline still count as resolved, but never as changed
 * or as a reason to invalidate texture-cache entries. */
static inline uint64_t T6FullTileResolutionPromotionEpoch(
    unsigned int pixelsResolved, int allSameSnapshot, uint64_t snapshotSeq,
    uint64_t materializedColorEpoch)
{
    if (pixelsResolved != T6_VRAM_TILE_SIZE * T6_VRAM_TILE_SIZE ||
        !allSameSnapshot || snapshotSeq <= materializedColorEpoch)
        return materializedColorEpoch;
    return snapshotSeq;
}

/* Unfiltered C0 keeps its existing contract: every resolved pixel is a pixel
 * actually coordinated by that read, so a partial read cannot claim a whole
 * tile. */
static inline uint64_t T6C0FullTilePromotionEpoch(
    int pixelsWritten, int allSameSnapshot, uint64_t snapshotSeq,
    uint64_t materializedColorEpoch)
{
    return T6FullTileResolutionPromotionEpoch(
        (unsigned int)pixelsWritten, allSameSnapshot, snapshotSeq,
        materializedColorEpoch);
}

/* A filtered pixel contributes to whole-tile resolution only when both raw
 * samples were readable and the baseline tile still proves the CPU-visible
 * color it was captured from.  Sampling-map and snapshot identity are kept
 * explicit so host tests can exercise every fail-closed input. */
static inline int T6FilteredResolutionEligible(
    int matchesRequiredSnapshot, int samplingMapMatches,
    int baselineTileFull, uint64_t baselineSeq, uint64_t cpuWriteEpoch,
    int pixelCompareSucceeded)
{
    return matchesRequiredSnapshot && samplingMapMatches &&
           baselineTileFull && baselineSeq != 0 &&
           baselineSeq >= cpuWriteEpoch && pixelCompareSucceeded;
}

/*
 * T6-B materialize session.  The session is the injectable core contract:
 * observe freezes the original hazard/required sequence before any capture
 * can replace a snapshot slot, validate re-proves only those original
 * hazards, and write/commit/invalidate run after the whole batch is safe.
 *
 * The session is intentionally compact: it stores only a touched-tile index
 * list plus the bitmap plan.  Per-tile sequences/map ids/slots live in a
 * reusable VramMaterializeWorkspace guarded by generation stamps, so a
 * single-tile dependency never allocates or scans the full 64x32 grid.
 */
enum
{
    T6_MAX_TOUCHED_TILES = T6_VRAM_TILE_X * T6_VRAM_TILE_Y
};

typedef struct VramMaterializeWorkspace
{
    uint64_t requiredSeq[T6_VRAM_TILE_Y][T6_VRAM_TILE_X];
    uint32_t requiredMapId[T6_VRAM_TILE_Y][T6_VRAM_TILE_X];
    int8_t requiredSlot[T6_VRAM_TILE_Y][T6_VRAM_TILE_X];
    unsigned int stamp[T6_VRAM_TILE_Y][T6_VRAM_TILE_X];
    unsigned int generation;
} VramMaterializeWorkspace;

typedef struct VramMaterializeSession
{
    VramMaterializePlan plan;
    VramMaterializeWorkspace *ws;
    uint16_t touched[T6_MAX_TOUCHED_TILES];
    int touchedCount;
    int overflow;
    int captureRequired;
    int captureConflict;
    int captureMappingKind;
    uint32_t captureMapId;
    int captureTileX;
    int captureTileY;
    int validateIterations;
    int writeIterations;
    int commitIterations;
} VramMaterializeSession;

/* Keep the hot-path stack object bounded; workspace is reusable/static. */
typedef char T6MaterializeSessionSizeGate[
    sizeof(VramMaterializeSession) <= 16384 ? 1 : -1];
typedef char T6MaterializeWorkspaceSizeGate[
    sizeof(VramMaterializeWorkspace) <= 65536 ? 1 : -1];

static inline void T6MaterializeSessionInit(VramMaterializeSession *s,
                                            VramMaterializeWorkspace *ws)
{
    if (s == NULL)
        return;
    memset(s, 0, sizeof(*s));
    T6MaterializePlanReset(&s->plan);
    s->ws = ws;
    if (ws != NULL)
    {
        if (++ws->generation == 0)
        {
            memset(ws->stamp, 0, sizeof(ws->stamp));
            ws->generation = 1;
        }
    }
}

static inline int T6MaterializeSessionHasEntry(
    const VramMaterializeSession *s, int tx, int ty)
{
    return s != NULL && s->ws != NULL &&
           s->ws->stamp[ty & 31][tx & 63] == s->ws->generation;
}

static inline uint64_t T6MaterializeSessionRequiredSeq(
    const VramMaterializeSession *s, int tx, int ty)
{
    return s->ws->requiredSeq[ty & 31][tx & 63];
}

static inline uint32_t T6MaterializeSessionRequiredMapId(
    const VramMaterializeSession *s, int tx, int ty)
{
    return s->ws->requiredMapId[ty & 31][tx & 63];
}

static inline int T6MaterializeSessionRequiredSlot(
    const VramMaterializeSession *s, int tx, int ty)
{
    return (int)s->ws->requiredSlot[ty & 31][tx & 63];
}

/*
 * Record one dependency tile.  The caller passes the effective EFB sequence
 * and snapshot sequence; when the tile is a hazard the required sequence is
 * frozen so a later capture cannot silently erase it.
 */
static inline void T6MaterializeSessionObserveTile(
    VramMaterializeSession *s, int tx, int ty,
    uint64_t psxColorSeq, uint64_t efbSeq, uint64_t snapshotSeq,
    uint32_t mapId, int snapshotSlot, int mappingKind,
    int physicalPresent, uint64_t physicalSeq)
{
    if (s == NULL || s->ws == NULL || tx < 0 || tx >= T6_VRAM_TILE_X ||
        ty < 0 || ty >= T6_VRAM_TILE_Y || s->plan.hazard[ty][tx])
        return;

    if (efbSeq == 0 || efbSeq <= psxColorSeq)
        return;

    if (s->touchedCount >= T6_MAX_TOUCHED_TILES)
    {
        s->overflow = 1;
        return;
    }

    T6MaterializePlanMarkHazard(&s->plan, tx, ty);
    s->ws->requiredSeq[ty][tx] = efbSeq;
    s->ws->requiredMapId[ty][tx] = mapId;
    s->ws->requiredSlot[ty][tx] = (int8_t)snapshotSlot;
    s->ws->stamp[ty][tx] = s->ws->generation;
    s->touched[s->touchedCount++] =
        (uint16_t)(ty * T6_VRAM_TILE_X + tx);

    if (physicalPresent && physicalSeq > snapshotSeq)
    {
        s->captureRequired = 1;
        if (s->captureMapId == 0)
        {
            s->captureMappingKind = mappingKind;
            s->captureMapId = mapId;
            s->captureTileX = tx;
            s->captureTileY = ty;
        }
        else if (s->captureMapId != mapId)
            s->captureConflict = 1;
    }
}

static inline int T6MaterializeSessionRequiresSlot(
    const VramMaterializeSession *s, int slot)
{
    int i;

    if (s == NULL)
        return 0;
    for (i = 0; i < s->touchedCount; i++)
    {
        int idx = s->touched[i];
        int tx = idx % T6_VRAM_TILE_X;
        int ty = idx / T6_VRAM_TILE_X;

        if (s->plan.hazard[ty][tx] &&
            T6MaterializeSessionRequiredSlot(s, tx, ty) == slot)
            return 1;
    }
    return 0;
}

/*
 * Pick a snapshot slot whose replacement cannot destroy evidence required by
 * the original hazard batch.  Returns 0 (fail closed) when both slots are
 * required, otherwise stores the safe slot in *outSlot.
 */
static inline int T6MaterializeSessionSelectCaptureSlot(
    const VramMaterializeSession *s, int oldestSlot, int *outSlot)
{
    int other;

    if (s == NULL || !s->captureRequired || outSlot == NULL)
        return 0;

    other = 1 - oldestSlot;
    if (T6MaterializeSessionRequiresSlot(s, oldestSlot) &&
        T6MaterializeSessionRequiresSlot(s, other))
        return 0;

    *outSlot = T6MaterializeSessionRequiresSlot(s, oldestSlot) ?
               other : oldestSlot;
    return 1;
}

/*
 * Validation callback fills the per-tile freshness input.  efbSeq is pinned
 * to the original required sequence by the core; the callback supplies the
 * current CPU/materialized epochs, snapshot sequence, hazards and baseline
 * state so EvaluateVramMaterializeTile can re-prove the original hazard.
 */
typedef int (*VramMaterializeValidateFn)(
    void *user, int tx, int ty, uint64_t requiredSeq,
    VramTileFreshnessInput *in);

static inline int T6MaterializeSessionValidate(
    VramMaterializeSession *s, VramMaterializeValidateFn validate,
    void *user)
{
    int i;

    if (s == NULL || s->ws == NULL || validate == NULL)
        return 0;

    for (i = 0; i < s->touchedCount; i++)
    {
        int idx = s->touched[i];
        int tx = idx % T6_VRAM_TILE_X;
        int ty = idx / T6_VRAM_TILE_X;
        VramTileFreshnessInput in;
        uint64_t requiredSeq;

        if (!s->plan.hazard[ty][tx])
            continue;
        s->validateIterations++;

        requiredSeq = T6MaterializeSessionRequiredSeq(s, tx, ty);
        memset(&in, 0, sizeof(in));
        in.efbSeq = requiredSeq;
        if (!validate(user, tx, ty, requiredSeq, &in))
            return 0;
        in.efbSeq = requiredSeq;

        if (EvaluateVramMaterializeTile(&in) != VRAM_FRESH_MATERIALIZED)
            return 0;
        T6MaterializePlanMarkResolved(&s->plan, tx, ty);
    }

    return T6MaterializePlanAllResolved(&s->plan);
}

/*
 * Write callback writes one resolved tile and marks changed tiles through the
 * plan.  It runs only after T6MaterializeSessionValidate returned 1.
 */
typedef void (*VramMaterializeWriteFn)(
    void *user, VramMaterializePlan *plan,
    int tx, int ty, uint64_t requiredSeq);

static inline int T6MaterializeSessionWrite(
    VramMaterializeSession *s, VramMaterializeWriteFn write, void *user)
{
    int i;

    if (s == NULL || write == NULL)
        return 0;

    for (i = 0; i < s->touchedCount; i++)
    {
        int idx = s->touched[i];
        int tx = idx % T6_VRAM_TILE_X;
        int ty = idx / T6_VRAM_TILE_X;

        if (!s->plan.resolved[ty][tx])
            continue;
        s->writeIterations++;
        write(user, &s->plan, tx, ty,
              T6MaterializeSessionRequiredSeq(s, tx, ty));
    }

    return 1;
}

/*
 * Commit callbacks: getSeq returns the snapshot sequence that proved the
 * tile and applyEpoch commits it directly to production/test epoch state.
 */
typedef uint64_t (*VramMaterializeSeqFn)(void *user, int tx, int ty);
typedef void (*VramMaterializeEpochFn)(void *user, int tx, int ty,
                                       uint64_t seq);

static inline void T6MaterializeSessionCommitEpoch(
    VramMaterializeSession *s, VramMaterializeSeqFn getSeq,
    VramMaterializeEpochFn applyEpoch, void *user)
{
    int i;

    if (s == NULL || getSeq == NULL || applyEpoch == NULL)
        return;

    for (i = 0; i < s->touchedCount; i++)
    {
        int idx = s->touched[i];
        int tx = idx % T6_VRAM_TILE_X;
        int ty = idx / T6_VRAM_TILE_X;

        if (!s->plan.resolved[ty][tx])
            continue;
        s->commitIterations++;
        applyEpoch(user, tx, ty, getSeq(user, tx, ty));
    }
}

static inline int T6MaterializeSessionInvalidateChanged(
    const VramMaterializeSession *s, VramTileRunCallback callback,
    void *user)
{
    if (s == NULL)
        return 0;
    return ForEachHorizontalTileRun(
        &s->plan.changed[0][0], T6_VRAM_TILE_X, T6_VRAM_TILE_Y,
        T6_VRAM_TILE_SIZE, callback, user);
}

/* Reusable workspace guard: a second, nested materialize attempt fails
 * closed instead of clobbering the in-flight session's generation data. */
static inline int T6WorkspaceBegin(int *depth)
{
    if (depth == NULL || *depth > 0)
        return 0;
    *depth = 1;
    return 1;
}

static inline void T6WorkspaceEnd(int *depth)
{
    if (depth != NULL && *depth > 0)
        *depth = 0;
}

enum
{
    T6_MAP_UNKNOWN = 0,
    T6_MAP_CURRENT = 1,
    T6_MAP_PREVIOUS = 2
};

static inline int T6TileIntersectsRect(int tx, int ty,
                                       int x0, int y0, int x1, int y1)
{
    int px0 = tx << 4;
    int py0 = ty << 4;

    return x0 < px0 + T6_VRAM_TILE_SIZE && px0 < x1 &&
           y0 < py0 + T6_VRAM_TILE_SIZE && py0 < y1;
}

/*
 * Per-tile map classification.  A tile intersecting both current and
 * previous rects is UNKNOWN; neither map may silently win the ownership.
 */
static inline int T6ClassifyTileMap(
    int tx, int ty,
    int activeX0, int activeY0, int activeX1, int activeY1,
    int prevX0, int prevY0, int prevX1, int prevY1,
    int previousValid)
{
    int inActive =
        activeX1 > activeX0 && activeY1 > activeY0 &&
        T6TileIntersectsRect(tx, ty, activeX0, activeY0,
                             activeX1, activeY1);
    int inPrevious =
        previousValid &&
        T6TileIntersectsRect(tx, ty, prevX0, prevY0, prevX1, prevY1);

    if (inActive && !inPrevious)
        return T6_MAP_CURRENT;
    if (inPrevious && !inActive)
        return T6_MAP_PREVIOUS;
    return T6_MAP_UNKNOWN;
}

/* Physical EFB ownership predicate.  INVALID_MAP_ID (0) represents the
 * pending-presented buffer that is captured under the previous map id. */
static inline int T6PhysicalOwnsTargetMap(int physMapId, int mappingKind,
                                          uint32_t targetMapId,
                                          uint32_t previousMapId)
{
    if (mappingKind == T6_MAP_CURRENT)
        return (uint32_t)physMapId == targetMapId;
    if (mappingKind == T6_MAP_PREVIOUS)
        return physMapId == 0 && targetMapId == previousMapId;
    return 0;
}

/* Only an UploadScreen transaction whose area covers the whole active map may
 * feed the active rebuild candidate; GX/clear already-FULL tiles plus a small
 * A0 patch must never qualify. */
static inline int T6UploadAreaCoversMap(int uploadX0, int uploadY0,
                                        int uploadX1, int uploadY1,
                                        int mapX0, int mapY0,
                                        int mapX1, int mapY1)
{
    return uploadX0 <= mapX0 && uploadY0 <= mapY0 &&
           uploadX1 >= mapX1 && uploadY1 >= mapY1;
}

static inline int T6RebuildCandidateEstablishes(int candidateValid,
                                                int candidateComplete,
                                                int coversMap)
{
    return candidateValid && !candidateComplete && coversMap;
}

typedef enum {
    T6_REASON_NONE = 0,
    T6_REASON_RGB24,
    T6_REASON_HAZARD,
    T6_REASON_UNKNOWN_MAP,
    T6_REASON_PARTIAL,
    T6_REASON_CAPTURE_FAIL,
    T6_REASON_BASELINE_STALE,
    T6_REASON_OVERFLOW,
    T6_REASON_REENTRANT,
    T6_REASON_COUNT
} T6BarrierReason;

/*
 * Strict qualification for the bounded A0 CPU-newer diagnostic probe.
 * The probe is useful only when it observes a real, older mapped EFB or
 * snapshot source and the just-finished A0 CPU epoch is strictly newer than
 * both that source and the previously materialized CPU color.  In that
 * state the generic read barrier must take the negative path: no hazard,
 * capture, required tile or unresolved reason.
 */
static inline int T6CpuNewerProbeQualifies(
    uint64_t provenanceSeq, uint64_t cpuWriteEpoch,
    uint64_t materializedColorEpoch, uint64_t physicalEfbSeq,
    uint64_t snapshotSeq, const VramMoveShadowDecision *decision)
{
    uint64_t oldSourceSeq = physicalEfbSeq > snapshotSeq ?
                            physicalEfbSeq : snapshotSeq;

    return decision != NULL &&
           provenanceSeq != 0 && cpuWriteEpoch == provenanceSeq &&
           oldSourceSeq != 0 && cpuWriteEpoch > oldSourceSeq &&
           cpuWriteEpoch > materializedColorEpoch &&
           decision->result == VRAM_FRESH_NO_ACTION &&
           decision->hazardTiles == 0 &&
           !decision->captureRequired && !decision->wouldCapture &&
           decision->capturePlanSlot == -1 &&
           !decision->captureBufferReady &&
           decision->requiredTiles == 0 &&
           decision->requiredSeq == 0 && decision->requiredMapId == 0 &&
           decision->unresolvedReason == T6_REASON_NONE;
}

/* DIAG provenance for the CPU-side write epoch that made a rebuild baseline
 * stale.  The fixed ring is host-testable and exact-sequence keyed: a missing
 * or overwritten entry never falls back to a newer/older write. */
typedef enum
{
    T6_CPU_WRITE_UNKNOWN = 0,
    T6_CPU_WRITE_A0,
    T6_CPU_WRITE_BLKFILL,
    T6_CPU_WRITE_MOVEIMAGE
} T6CpuWriteKind;

enum
{
    T6_CPU_WRITE_FLAG_UPLOAD_PENDING = 1u << 0,
    T6_CPU_WRITE_FLAG_FULL_REBUILD = 1u << 1,
    T6_CPU_WRITE_FLAG_CHECK_CALLED = 1u << 2,
    T6_CPU_WRITE_FLAG_UPLOAD_SUCCEEDED = 1u << 3,
    T6_CPU_WRITE_FLAG_BASELINE_ESTABLISHED = 1u << 4,
    T6_CPU_WRITE_FLAG_UPLOAD_DEFERRED = 1u << 5,
    T6_CPU_WRITE_PROVENANCE_CAPACITY = 64
};

typedef struct T6CpuWriteProvenance
{
    uint64_t seq;
    int kind;
    int x, y, w, h;
    unsigned int flags;
    uint64_t baselineSeq;
    uint32_t baselineMapId;
    /* A0 transfer lifecycle captured at FinishedVRAMWrite().  These fields
     * are diagnostic only and distinguish a stale transfer/duplicate finish
     * from a normal primLoadImage -> upload flow. */
    unsigned int a0Generation;
    unsigned int a0FinishSerial;
    unsigned int a0DmaSerial;
    unsigned int a0ArmDmaSerial;
    int a0Armed;
    int a0NeedUpload;
    int a0WriteMode;
    int a0RowsRemaining;
    int a0ColsRemaining;
    int a0ArmX, a0ArmY, a0ArmW, a0ArmH;
    int a0LastDmaMode;
    int a0LastDmaOldWriteMode;
    int a0LastDmaNewWriteMode;
} T6CpuWriteProvenance;

typedef struct T6CpuWriteProvenanceRing
{
    T6CpuWriteProvenance entry[T6_CPU_WRITE_PROVENANCE_CAPACITY];
    unsigned int next;
    unsigned int count;
} T6CpuWriteProvenanceRing;

static inline void T6CpuWriteProvenanceInit(
    T6CpuWriteProvenanceRing *ring)
{
    if (ring != NULL)
        memset(ring, 0, sizeof(*ring));
}

static inline void T6CpuWriteProvenanceRecord(
    T6CpuWriteProvenanceRing *ring, uint64_t seq, int kind,
    int x, int y, int w, int h, unsigned int flags)
{
    T6CpuWriteProvenance *entry;

    if (ring == NULL || seq == 0 || w <= 0 || h <= 0)
        return;
    entry = &ring->entry[ring->next];
    /* Clear the complete reused slot so no post-outcome or lifecycle field
     * can leak from an overwritten CPU write. */
    memset(entry, 0, sizeof(*entry));
    entry->seq = seq;
    entry->kind = kind;
    entry->x = x;
    entry->y = y;
    entry->w = w;
    entry->h = h;
    entry->flags = flags;
    ring->next = (ring->next + 1u) % T6_CPU_WRITE_PROVENANCE_CAPACITY;
    if (ring->count < T6_CPU_WRITE_PROVENANCE_CAPACITY)
        ring->count++;
}

/* Add post-write outcome without ever associating it with an adjacent
 * timeline event.  A missing/overwritten exact sequence remains unknown. */
static inline int T6CpuWriteProvenanceUpdate(
    T6CpuWriteProvenanceRing *ring, uint64_t seq,
    unsigned int addFlags, uint64_t baselineSeq,
    uint32_t baselineMapId)
{
    unsigned int i;

    if (ring == NULL || seq == 0)
        return 0;
    for (i = 0; i < ring->count; i++)
    {
        unsigned int idx =
            (ring->next + T6_CPU_WRITE_PROVENANCE_CAPACITY - 1u - i) %
            T6_CPU_WRITE_PROVENANCE_CAPACITY;
        T6CpuWriteProvenance *entry = &ring->entry[idx];

        if (entry->seq == seq)
        {
            entry->flags |= addFlags;
            entry->baselineSeq = baselineSeq;
            entry->baselineMapId = baselineMapId;
            return 1;
        }
    }
    return 0;
}

/* Attach state from the exact A0 finish event.  Newest-exact lookup prevents
 * an adjacent event from being credited after ring wrap or duplicate seq. */
static inline int T6CpuWriteProvenanceUpdateA0Flow(
    T6CpuWriteProvenanceRing *ring, uint64_t seq,
    unsigned int generation, unsigned int finishSerial,
    unsigned int dmaSerial, unsigned int armDmaSerial,
    int armed, int needUpload, int writeMode,
    int rowsRemaining, int colsRemaining,
    int armX, int armY, int armW, int armH,
    int lastDmaMode, int lastDmaOldWriteMode, int lastDmaNewWriteMode)
{
    unsigned int i;

    if (ring == NULL || seq == 0)
        return 0;
    for (i = 0; i < ring->count; i++)
    {
        unsigned int idx =
            (ring->next + T6_CPU_WRITE_PROVENANCE_CAPACITY - 1u - i) %
            T6_CPU_WRITE_PROVENANCE_CAPACITY;
        T6CpuWriteProvenance *entry = &ring->entry[idx];

        if (entry->seq == seq)
        {
            entry->a0Generation = generation;
            entry->a0FinishSerial = finishSerial;
            entry->a0DmaSerial = dmaSerial;
            entry->a0ArmDmaSerial = armDmaSerial;
            entry->a0Armed = armed;
            entry->a0NeedUpload = needUpload;
            entry->a0WriteMode = writeMode;
            entry->a0RowsRemaining = rowsRemaining;
            entry->a0ColsRemaining = colsRemaining;
            entry->a0ArmX = armX;
            entry->a0ArmY = armY;
            entry->a0ArmW = armW;
            entry->a0ArmH = armH;
            entry->a0LastDmaMode = lastDmaMode;
            entry->a0LastDmaOldWriteMode = lastDmaOldWriteMode;
            entry->a0LastDmaNewWriteMode = lastDmaNewWriteMode;
            return 1;
        }
    }
    return 0;
}

static inline int T6CpuWriteProvenanceFind(
    const T6CpuWriteProvenanceRing *ring, uint64_t seq,
    T6CpuWriteProvenance *out)
{
    unsigned int i;

    if (ring == NULL || seq == 0)
        return 0;
    for (i = 0; i < ring->count; i++)
    {
        unsigned int idx =
            (ring->next + T6_CPU_WRITE_PROVENANCE_CAPACITY - 1u - i) %
            T6_CPU_WRITE_PROVENANCE_CAPACITY;

        if (ring->entry[idx].seq == seq)
        {
            if (out != NULL)
                *out = ring->entry[idx];
            return 1;
        }
    }
    return 0;
}

/* A synthetic capture candidate can only satisfy tiles from the map that
 * the real capture will own.  Keep this explicit so a multi-map dependency
 * cannot borrow capture/baseline evidence across map identities. */
static inline int T6MoveCaptureCandidateMatchesMap(
    uint32_t captureMapId, uint32_t requiredMapId)
{
    return captureMapId != 0 && captureMapId == requiredMapId;
}

/* A read-only shadow cannot prove that a new MEM2 allocation will succeed.
 * Only an already allocated target slot with sufficient capacity may be
 * treated as a guaranteed capture for equivalence gating. */
static inline int T6MoveCaptureBufferReady(
    int pixelsPresent, int slotSize, int requiredSize)
{
    return pixelsPresent && requiredSize > 0 &&
           slotSize >= requiredSize;
}

/* Capture-eligibility classification shared by production diagnostics and
 * host tests: RGB24/hazard/unknown map must not collapse into CAPTURE_FAIL. */
static inline int T6ClassifyCaptureFailureReason(int rgb24,
                                                 int contaminated,
                                                 int mixedMapping,
                                                 int untrackedEfb,
                                                 int unknownMap)
{
    if (unknownMap)
        return T6_REASON_UNKNOWN_MAP;
    if (rgb24)
        return T6_REASON_RGB24;
    if (contaminated || mixedMapping || untrackedEfb)
        return T6_REASON_HAZARD;
    return T6_REASON_CAPTURE_FAIL;
}

typedef struct T6CaptureDiagInput
{
    int rgb24;
    int contaminated;
    int mixedMapping;
    int untrackedEfb;
    int unknownMap;
} T6CaptureDiagInput;

/* Build the capture diagnostic input from the actual capture source.  For a
 * current-map capture, pending-presented RGB24 state is irrelevant; only a
 * previous-map capture reads the pending map. */
static inline void T6CaptureDiagFill(T6CaptureDiagInput *in,
                                     int globalRgb24, int activeRgb24,
                                     int pendingRgb24, int mappingKind,
                                     int contaminated, int mixedMapping,
                                     int untrackedEfb, int unknownMap)
{
    if (in == NULL)
        return;
    in->rgb24 = globalRgb24 ||
                (mappingKind == T6_MAP_PREVIOUS ? pendingRgb24 :
                                                  activeRgb24);
    in->contaminated = contaminated;
    in->mixedMapping = mixedMapping;
    in->untrackedEfb = untrackedEfb;
    in->unknownMap = unknownMap;
}

static inline int T6ClassifyCaptureFailure(const T6CaptureDiagInput *in)
{
    if (in == NULL)
        return T6_REASON_CAPTURE_FAIL;
    return T6ClassifyCaptureFailureReason(
        in->rgb24, in->contaminated, in->mixedMapping,
        in->untrackedEfb, in->unknownMap);
}

/* Shared CopyTex stat helpers: production passes its DIAG globals, host tests
 * pass local counters, so the increment/reset contract is testable. */
static inline void T6CopyTexBeginCall(unsigned int *thisCall)
{
    if (thisCall != NULL)
        *thisCall = 0;
}

static inline void T6CopyTexCapture(unsigned int *thisCall,
                                    unsigned int *total)
{
    if (thisCall != NULL)
        (*thisCall)++;
    if (total != NULL)
        (*total)++;
}

typedef void (*VramReadTileCallback)(void *user, int tx, int ty);

/*
 * Enumerate every 16x16 VRAM tile covered by the dependency.  Overlapping
 * dependency rects may invoke the callback more than once for the same tile;
 * observers must be idempotent or aggregate per tile.
 */
static inline int ForEachVramReadDependencyTile(
    const VramReadDependency *dep, int vramWidth, int vramHeight,
    VramReadTileCallback callback, void *user)
{
    int i, count = 0;

    if (dep == NULL || callback == NULL ||
        vramWidth != 1024 || vramHeight != 512 ||
        dep->count == 0 || dep->count > T6_DEP_RECT_CAPACITY)
        return 0;

    /* Validate every rect before invoking the callback. */
    for (i = 0; i < (int)dep->count; i++)
    {
        const VramRect *r = &dep->rect[i];
        if (VramRectIsEmpty(r) || r->x0 < 0 || r->y0 < 0 ||
            r->x1 > 1024 || r->y1 > 512)
            return 0;
    }

    for (i = 0; i < (int)dep->count; i++)
    {
        const VramRect *r = &dep->rect[i];
        int tx0 = r->x0 >> 4;
        int tx1 = (r->x1 - 1) >> 4;
        int ty0 = r->y0 >> 4;
        int ty1 = (r->y1 - 1) >> 4;
        int tx, ty;

        for (ty = ty0; ty <= ty1; ty++)
            for (tx = tx0; tx <= tx1; tx++)
            {
                callback(user, tx & 63, ty & 31);
                count++;
            }
    }

    return count;
}

/* P2-F3: generic 成功后 post 决策是否构成 takeover regression（每个
 * MoveImage 最多计一次）。generic 未成功（UNRESOLVED）是预期的
 * fail-closed 状态，不计入 success-post regression，由 takeUnresolved
 * 统计；generic 成功但 post 不是 clean
 * NO_ACTION（result 非 NO_ACTION、reason 非 NONE 或 hazard 非零）才计。
 * 定义在 T6Reason 枚举之后（需引用 T6_REASON_NONE）。 */
static inline int T6MovePostGenericRegression(
    int genericResult, int postResult, int postReason,
    unsigned int postHazard)
{
    if (genericResult != VRAM_FRESH_NO_ACTION &&
        genericResult != VRAM_FRESH_MATERIALIZED)
        return 0;
    return postResult != VRAM_FRESH_NO_ACTION ||
           postReason != T6_REASON_NONE ||
           postHazard != 0;
}

#ifdef __cplusplus
}
#endif

#endif /* GPU_TEXTURE_READ_BARRIER_H */
