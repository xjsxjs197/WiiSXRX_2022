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

/* Explicit output reset used by every changedTilesOut early-return path. */
static inline void T6ResetChangedTilesOutput(unsigned int *out)
{
    if (out != NULL)
        *out = 0;
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
 * C0/T6 cross guard: a C0 snapshot whose tile sequence is older than the
 * materialized color epoch must not overwrite the low 15 bits.  Equal
 * sequences are allowed per the design review note.
 */
static inline int T6C0ColorCommitAllowed(uint64_t snapshotSeq,
                                         uint64_t materializedColorEpoch)
{
    return snapshotSeq >= materializedColorEpoch;
}

/*
 * Only a C0 read that wrote all 256 pixels of a tile from one snapshot may
 * promote the materialized color epoch; partial C0 reads never claim a whole
 * tile.
 */
static inline uint64_t T6C0FullTilePromotionEpoch(
    int pixelsWritten, int allSameSnapshot, uint64_t snapshotSeq,
    uint64_t materializedColorEpoch)
{
    if (pixelsWritten != T6_VRAM_TILE_SIZE * T6_VRAM_TILE_SIZE ||
        !allSameSnapshot || snapshotSeq <= materializedColorEpoch)
        return materializedColorEpoch;
    return snapshotSeq;
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

#ifdef __cplusplus
}
#endif

#endif /* GPU_TEXTURE_READ_BARRIER_H */
