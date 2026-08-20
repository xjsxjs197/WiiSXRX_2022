#ifndef GPU_TEXTURE_READ_BARRIER_H
#define GPU_TEXTURE_READ_BARRIER_H

#include <stdint.h>

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
    T6_DEP_RECT_CAPACITY = 12
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
    int efbCoverFull;
    int rgb24;
    int contaminated;
    int mixedMapping;
    int untrackedEfb;
    int hasValidSnapshot;
} VramTileFreshnessInput;

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
