/*
 * gpuVramReadback.inc - C0 VRAM readback state machine (v11)
 *
 * Included by gpuPlugin.c after gpuDraw.c / gpuTexture.c and before
 * gpuPrim.c so the whole GlesGpu
 * translation unit can use the helpers below.
 */

#ifndef GPU_VRAM_READBACK_INC
#define GPU_VRAM_READBACK_INC

#include <string.h>
#ifdef DISP_DEBUG
#include <ogc/lwp_watchdog.h>
#endif
#include "gpuTextureReadBarrier.h"

#if defined(DISP_DEBUG) && defined(TEXTURE_READ_BARRIER_DIAG)
#define T6_BARRIER_DIAG 1
#else
#define T6_BARRIER_DIAG 0
#endif

#define EFB_DRAW_CONTEXT_DEPTH 8
#define VRAM_READBACK_TILE_X 64
#define VRAM_READBACK_TILE_Y 32
#define VRAM_READBACK_TILE_SIZE 16
#define INVALID_MAP_ID 0u

typedef enum {
    EFB_TILE_NONE,
    EFB_TILE_PARTIAL,
    EFB_TILE_FULL
} EfbTileCover;

typedef enum {
    MAPPING_CURRENT,
    MAPPING_PREVIOUS,
    MAPPING_UNKNOWN
} MappingKind;

typedef enum {
    SNAPSHOT_SOURCE_NONE,
    SNAPSHOT_SOURCE_CURRENT_EFB,
    SNAPSHOT_SOURCE_PRESENTED,
    SNAPSHOT_SOURCE_UPLOAD_REBUILD
} SnapshotSource;

typedef enum {
    READBACK_IDLE,
    READBACK_PENDING,
    READBACK_DONE
} ReadbackState;

typedef struct {
    uint64_t seq;
    EfbTileCover cover;
} EfbTileState;

typedef struct {
    int active;
    int rectSet;
    int x0, y0;
    int x1, y1;
    uint32_t mapId;
    int submittedAny;
    uint64_t lastSeq;
    EfbTileCover coverage;
    uint64_t fixedSeq;
    int externalRebuild;
    int presentationRebuild;
    int pendingRender;
    int rebuildUpload;
} EfbDrawContext;

typedef struct {
    void *pixels;
    int   valid;
    int   size;
    int   texWidth;
    int   texHeight;

    int vram_x, vram_y, vram_w, vram_h;
    int viewport_x, viewport_y, viewport_w, viewport_h;

    RECT ratio;
    uint32_t map_id;
    SnapshotSource source;

    /* A previous-page C0 fallback may only own the exact read rectangle.
     * This avoids promoting edge tiles, whose other pixels may still belong
     * to CPU VRAM, to full snapshot ownership. */
    int exactRectValid;
    int exact_x0, exact_y0;
    int exact_x1, exact_y1;

    EfbTileState efbTile[VRAM_READBACK_TILE_Y][VRAM_READBACK_TILE_X];
    uint64_t capturedContentSeq;
    uint64_t captureOrder;
} GXFrameSnapshot;

typedef struct {
    int map_valid;
    int content_valid;
    int content_dirty;

    int vram_x0, vram_y0;
    int vram_x1, vram_y1;
    int viewport_x0, viewport_y0;
    int viewport_x1, viewport_y1;

    RECT ratio;
    uint32_t map_id;
    int rgb24;
    uint64_t contentWriteSeq;
} GXDisplayMap;

typedef struct {
    int valid;
    int complete;
    GXDisplayMap map;
    EfbTileState tile[VRAM_READBACK_TILE_Y][VRAM_READBACK_TILE_X];
} EfbRebuildCandidate;

static EfbDrawContext g_efbContextStack[EFB_DRAW_CONTEXT_DEPTH];
static int g_efbContextDepth;
static int g_efbOverflowDepth;

static GXFrameSnapshot g_snapshot[2];
static int g_liveSlot = 0;
static int g_prevSlot = 1;

/* Single CPU-authoritative rebuild baseline for T6 delta merges.  It is
 * captured when a complete map has been rebuilt from psxVuw, so materialize
 * can copy only the pixels changed by later GX draws. */
static GXFrameSnapshot g_rebuildBaseline;

static GXDisplayMap g_activeMap;
static EfbRebuildCandidate g_rebuildCandidate;
static EfbRebuildCandidate g_activeRebuildCandidate;
static GXDisplayMap g_pendingPresentedMap;
static int g_pendingPresentedReady;
static uint64_t g_pendingPresentedCaptureOrder;

static uint32_t g_mapId;
static uint32_t g_prevMapId;
static uint64_t g_writeSeq;
static uint64_t g_captureOrder;

static uint64_t g_psxWriteTileEpoch[VRAM_READBACK_TILE_Y][VRAM_READBACK_TILE_X];
static uint64_t g_psxMaterializedColorTileEpoch[VRAM_READBACK_TILE_Y][VRAM_READBACK_TILE_X];
static VramMaterializeWorkspace g_t6Workspace;
static int g_t6WorkspaceDepth;
static int g_vramReadBarrierDepth;
static EfbTileState g_efbTileState[VRAM_READBACK_TILE_Y][VRAM_READBACK_TILE_X];
static EfbTileState g_pendingPresentedTile[VRAM_READBACK_TILE_Y][VRAM_READBACK_TILE_X];

static BOOL g_efbContaminated;
static unsigned int g_debugDrawSubmitted;
static BOOL g_mixedMappingSeen;
static BOOL g_untrackedEfbWrite;

static ReadbackState g_readbackState;
static int g_lastCaptureResult;
static MappingKind g_lastReadMapping;
#ifdef DISP_DEBUG
static unsigned int g_lastMergedPixels;
static unsigned int g_lastMergedChangedPixels;
static unsigned int g_lastMergedMaskOnlyPixels;
static unsigned int g_lastMergedRgbChangedPixels;
static unsigned int g_lastMergeOldMaskPixels;
static unsigned int g_lastMergeNewMaskPixels;
static unsigned int g_lastMergeOldHash;
static unsigned int g_lastMergeNewHash;
static unsigned int g_lastMergedCurrentPixels;
static unsigned int g_lastMergedPresentedPixels;
static unsigned int g_lastMergedRebuildPixels;
static unsigned int g_lastReadbackInvalidateRuns;
static unsigned int g_lastReadbackPaletteEntryChecks;
static unsigned int g_lastReadbackTotalInvalidated;
static unsigned int g_lastReadbackInvalidateUs;
#if T6_BARRIER_DIAG
static unsigned int g_vramReadBarrierCalls;
static unsigned int g_vramReadBarrierNoAction;
static unsigned int g_vramReadBarrierMaterialized;
static unsigned int g_vramReadBarrierUnresolved;
static unsigned int g_vramReadBarrierReentrant;
static int g_vramReadBarrierLastReason;
static unsigned int g_vramReadBarrierCopyTex;
static unsigned int g_vramReadBarrierCopyTexTotal;
static unsigned int g_vramReadBarrierChangedTiles;
static unsigned int g_vramReadBarrierInvalidateRuns;
static unsigned int g_vramReadBarrierSuccessLogs;
static unsigned int g_vramReadBarrierReasonLogs[T6_REASON_COUNT];
#endif
#endif

static int g_displayMapTxDepth;
static BOOL g_displayMapTxChanged;

#ifdef DISP_DEBUG
static GXFrameSnapshot g_diagBaseline;
static unsigned int g_diagCycle;
static unsigned int g_diagBaselineDrawCount;
static int g_diagBaselinePending;
static uint64_t g_diagLastActiveWriteSeq;
static uint64_t g_diagLastExternalLogSeq;
#endif

#define LIVE_SNAP() (&g_snapshot[g_liveSlot])
#define PREV_SNAP() (&g_snapshot[g_prevSlot])

extern void RestoreDispCopyInfo(void);
static inline unsigned short GXRGB5A3ToPSX15(unsigned short gx);
static int SnapshotSourcePriority(const GXFrameSnapshot *snap,
                                  MappingKind mapping);
static int ContainsPixel(const GXFrameSnapshot *snap, int px, int py);
#ifdef DISP_DEBUG
static void DebugComparePresentedToBaseline(const GXFrameSnapshot *snap);
#endif

static BOOL ReadbackEnabled(void)
{
	return (dwActFixes & AUTO_FIX_VRAM_READBACK) != 0;
}

#ifdef DISP_DEBUG
static void DebugLogVramHalf(const char *tag, int x, int y, int w, int h)
{
    unsigned int nonZero[2] = {0, 0};
    unsigned int hash[2] = {2166136261u, 2166136261u};
    int yy, xx;

    if (!ReadbackEnabled() || w <= 0 || h <= 0)
        return;

    for (yy = 0; yy < h; yy++)
    {
        int half = (yy < (h / 2)) ? 0 : 1;
        int py = (y + yy) & iGPUHeightMask;

        for (xx = 0; xx < w; xx++)
        {
            int px = (x + xx) & 0x3ff;
            unsigned short value = GETLE16(psxVuw + (py << 10) + px);

            if (value & 0x7fff)
                nonZero[half]++;
            hash[half] = (hash[half] ^ value) * 16777619u;
        }
    }

    sprintf(txtbuffer,
            "VRH %s rect=%d,%d %dx%d split=%d "
            "top=%u/%08X bottom=%u/%08X\r\n",
            tag, x, y, w, h, y + h / 2,
            nonZero[0], hash[0], nonZero[1], hash[1]);
    writeLogFile(txtbuffer);
}
#endif

static void ActiveMapVRamRect(int *x0, int *y0, int *x1, int *y1)
{
    *x0 = g_activeMap.vram_x0;
    *y0 = g_activeMap.vram_y0;
    *x1 = g_activeMap.vram_x1;
    *y1 = g_activeMap.vram_y1;
}

static EfbDrawContext *TopEfbContext(void)
{
    if (g_efbOverflowDepth > 0)
        return NULL;

    if (g_efbContextDepth <= 0)
        return NULL;

    return &g_efbContextStack[g_efbContextDepth - 1];
}

static int BeginEfbDrawContext(void)
{
    if (!ReadbackEnabled())
        return 0;

    if (g_efbContextDepth >= EFB_DRAW_CONTEXT_DEPTH)
    {
        g_efbOverflowDepth++;
        return 0;
    }

    EfbDrawContext *ctx = &g_efbContextStack[g_efbContextDepth++];
    memset(ctx, 0, sizeof(*ctx));
    ctx->active = 1;
    ctx->coverage = EFB_TILE_PARTIAL;
    return 1;
}

static void EndEfbDrawContext(void)
{
    if (!ReadbackEnabled())
        return;

    if (g_efbOverflowDepth > 0)
    {
        g_efbOverflowDepth--;
        return;
    }

    if (g_efbContextDepth > 0)
    {
        memset(&g_efbContextStack[--g_efbContextDepth],
               0, sizeof(EfbDrawContext));
    }
}

static void SetEfbDrawContextRect(int x0, int y0,
                                  int x1, int y1,
                                  uint32_t mapId)
{
    EfbDrawContext *ctx = TopEfbContext();

    if (ctx == NULL)
        return;

    ctx->x0 = x0;
    ctx->y0 = y0;
    ctx->x1 = x1;
    ctx->y1 = y1;
    ctx->mapId = mapId;
    ctx->rectSet = 1;
}

static void SetEfbDrawContextFromVertices(int count, int addDrawOffset)
{
    EfbDrawContext *ctx = TopEfbContext();
    const GXDisplayMap *drawMap = &g_activeMap;
    int lx[4], ly[4];
    int i, x0, y0, x1, y1;
    int usePending = 0;

    if (ctx == NULL || count < 1)
        return;

    lx[0] = lx0; ly[0] = ly0;
    lx[1] = lx1; ly[1] = ly1;
    lx[2] = lx2; ly[2] = ly2;
    lx[3] = lx3; ly[3] = ly3;

    x0 = x1 = lx[0] + (addDrawOffset ? PSXDisplay.DrawOffset.x : 0);
    y0 = y1 = ly[0] + (addDrawOffset ? PSXDisplay.DrawOffset.y : 0);

    for (i = 1; i < count; i++)
    {
        int xi = lx[i] + (addDrawOffset ? PSXDisplay.DrawOffset.x : 0);
        int yi = ly[i] + (addDrawOffset ? PSXDisplay.DrawOffset.y : 0);

        if (xi < x0) x0 = xi;
        if (xi > x1) x1 = xi;
        if (yi < y0) y0 = yi;
        if (yi > y1) y1 = yi;
    }

    /* Convert inclusive max vertex to half-open upper bound so
     * vertical/horizontal lines and single-pixel edges still cover a tile. */
    x1++;
    y1++;

    if (x0 < PSXDisplay.DrawArea.x0) x0 = PSXDisplay.DrawArea.x0;
    if (y0 < PSXDisplay.DrawArea.y0) y0 = PSXDisplay.DrawArea.y0;
    if (x1 > PSXDisplay.DrawArea.x1 + 1) x1 = PSXDisplay.DrawArea.x1 + 1;
    if (y1 > PSXDisplay.DrawArea.y1 + 1) y1 = PSXDisplay.DrawArea.y1 + 1;

    if (g_pendingPresentedReady &&
        x0 < g_pendingPresentedMap.vram_x1 &&
        x1 > g_pendingPresentedMap.vram_x0 &&
        y0 < g_pendingPresentedMap.vram_y1 &&
        y1 > g_pendingPresentedMap.vram_y0 &&
        (g_pendingPresentedMap.vram_x0 != g_activeMap.vram_x0 ||
         g_pendingPresentedMap.vram_y0 != g_activeMap.vram_y0 ||
         g_pendingPresentedMap.vram_x1 != g_activeMap.vram_x1 ||
         g_pendingPresentedMap.vram_y1 != g_activeMap.vram_y1))
    {
        drawMap = &g_pendingPresentedMap;
        usePending = 1;
    }

    if (x0 < drawMap->vram_x0) x0 = drawMap->vram_x0;
    if (y0 < drawMap->vram_y0) y0 = drawMap->vram_y0;
    if (x1 > drawMap->vram_x1) x1 = drawMap->vram_x1;
    if (y1 > drawMap->vram_y1) y1 = drawMap->vram_y1;

    ctx->x0 = x0;
    ctx->y0 = y0;
    ctx->x1 = x1;
    ctx->y1 = y1;
    ctx->mapId = drawMap->map_id;
    if (usePending && ctx->mapId == INVALID_MAP_ID)
    {
        if (drawMap->vram_x0 == PreviousPSXDisplay.DisplayPosition.x &&
            drawMap->vram_y0 == PreviousPSXDisplay.DisplayPosition.y &&
            drawMap->vram_x1 == PreviousPSXDisplay.DisplayEnd.x &&
            drawMap->vram_y1 == PreviousPSXDisplay.DisplayEnd.y)
            ctx->mapId = g_prevMapId;
        else if (drawMap->vram_x0 == g_activeMap.vram_x0 &&
                 drawMap->vram_y0 == g_activeMap.vram_y0 &&
                 drawMap->vram_x1 == g_activeMap.vram_x1 &&
                 drawMap->vram_y1 == g_activeMap.vram_y1)
            ctx->mapId = g_activeMap.map_id;
    }
    ctx->rectSet = 1;
    ctx->coverage = EFB_TILE_PARTIAL;
    ctx->pendingRender = usePending;
}

static uint64_t BeginCommandWrite(void)
{
    if (!ReadbackEnabled())
        return 0;

    return ++g_writeSeq;
}

static void MarkCpuVramWriteWithSeq(uint64_t seq,
                                    int x, int y, int w, int h)
{
    if (!ReadbackEnabled())
        return;

    if (w <= 0 || h <= 0)
        return;

    while (w > 0)
    {
        int x1 = (x + w <= 1024) ? x + w : 1024;
        int tx0 = x >> 4, tx1 = (x1 - 1) >> 4;
        int ty0 = y >> 4, ty1 = (y + h - 1) >> 4;
        int ty, tx;

        for (ty = ty0; ty <= ty1; ty++)
            for (tx = tx0; tx <= tx1; tx++)
                g_psxWriteTileEpoch[ty & 31][tx & 63] = seq;

        w -= x1 - x;
        x = 0;
    }
}

static void MarkCpuVramWrite(int x, int y, int w, int h)
{
    if (!ReadbackEnabled())
        return;

    MarkCpuVramWriteWithSeq(++g_writeSeq, x, y, w, h);
}

static void MarkEfbTilesPartial(uint64_t seq,
                                int x0, int y0, int x1, int y1)
{
    while (x1 > x0)
    {
        int ex = (x1 <= 1024) ? x1 : 1024;
        int tx0 = x0 >> 4, tx1 = (ex - 1) >> 4;
        int ty0 = y0 >> 4, ty1 = (y1 - 1) >> 4;
        int ty, tx;

        for (ty = ty0; ty <= ty1; ty++)
            for (tx = tx0; tx <= tx1; tx++)
            {
                EfbTileState *st = &g_efbTileState[ty & 31][tx & 63];

                if (st->cover == EFB_TILE_NONE)
                    st->cover = EFB_TILE_PARTIAL;

                /* A partial draw over a fully reconstructed tile leaves the
                 * whole tile available in EFB.  Only its newest epoch changes. */
                st->seq = seq;
            }

        x0 = 0;
        x1 -= ex;
    }
}

static void MarkEfbTilesFull(uint64_t seq,
                             int x0, int y0, int x1, int y1)
{
    int tx0, tx1, ty0, ty1, ty, tx;

    MarkEfbTilesPartial(seq, x0, y0, x1, y1);

    tx0 = (x0 + 15) >> 4;
    tx1 = (x1 >> 4) - 1;
    ty0 = (y0 + 15) >> 4;
    ty1 = (y1 >> 4) - 1;

    for (ty = ty0; ty <= ty1; ty++)
        for (tx = tx0; tx <= tx1; tx++)
        {
            EfbTileState *st = &g_efbTileState[ty & 31][tx & 63];
            st->cover = EFB_TILE_FULL;
            st->seq = seq;
        }
}

static void MarkEfbTilesUpload(uint64_t seq,
                               int x0, int y0, int x1, int y1)
{
    MarkEfbTilesFull(seq, x0, y0, x1, y1);
}

static void MarkTileArrayFull(EfbTileState tile[VRAM_READBACK_TILE_Y]
                                               [VRAM_READBACK_TILE_X],
                              uint64_t seq,
                              int x0, int y0, int x1, int y1)
{
    int tx0 = (x0 + 15) >> 4;
    int tx1 = (x1 >> 4) - 1;
    int ty0 = (y0 + 15) >> 4;
    int ty1 = (y1 >> 4) - 1;
    int ty, tx;

    for (ty = ty0; ty <= ty1; ty++)
        for (tx = tx0; tx <= tx1; tx++)
        {
            EfbTileState *st = &tile[ty & 31][tx & 63];
            st->cover = EFB_TILE_FULL;
            st->seq = seq;
        }
}

static void MarkTileArrayPartial(EfbTileState tile[VRAM_READBACK_TILE_Y]
                                                   [VRAM_READBACK_TILE_X],
                                 uint64_t seq,
                                 int x0, int y0, int x1, int y1)
{
    int tx0, tx1, ty0, ty1, ty, tx;

    if (x1 <= x0 || y1 <= y0)
        return;

    tx0 = x0 >> 4;
    tx1 = (x1 - 1) >> 4;
    ty0 = y0 >> 4;
    ty1 = (y1 - 1) >> 4;

    for (ty = ty0; ty <= ty1; ty++)
        for (tx = tx0; tx <= tx1; tx++)
        {
            EfbTileState *st = &tile[ty & 31][tx & 63];
            if (st->cover == EFB_TILE_NONE)
                st->cover = EFB_TILE_PARTIAL;
            st->seq = seq;
        }
}

static int TileArrayCoversMap(
    EfbTileState tile[VRAM_READBACK_TILE_Y][VRAM_READBACK_TILE_X],
    const GXDisplayMap *map)
{
    int tx0 = (map->vram_x0 + 15) >> 4;
    int tx1 = (map->vram_x1 >> 4) - 1;
    int ty0 = (map->vram_y0 + 15) >> 4;
    int ty1 = (map->vram_y1 >> 4) - 1;
    int ty, tx;

    if (tx1 < tx0 || ty1 < ty0)
        return 0;

    for (ty = ty0; ty <= ty1; ty++)
        for (tx = tx0; tx <= tx1; tx++)
            if (tile[ty & 31][tx & 63].cover != EFB_TILE_FULL)
                return 0;

    return 1;
}

static void ResetRebuildCandidate(void)
{
    memset(&g_rebuildCandidate, 0, sizeof(g_rebuildCandidate));
}

static void ResetActiveRebuildCandidate(void)
{
    memset(&g_activeRebuildCandidate, 0, sizeof(g_activeRebuildCandidate));
}

static void PreparePreviousRebuildCandidate(uint32_t mapId)
{
    int same = g_rebuildCandidate.valid &&
               g_rebuildCandidate.map.map_id == mapId &&
               g_rebuildCandidate.map.vram_x0 == PreviousPSXDisplay.DisplayPosition.x &&
               g_rebuildCandidate.map.vram_y0 == PreviousPSXDisplay.DisplayPosition.y &&
               g_rebuildCandidate.map.vram_x1 == PreviousPSXDisplay.DisplayEnd.x &&
               g_rebuildCandidate.map.vram_y1 == PreviousPSXDisplay.DisplayEnd.y;

    if (same)
        return;

    ResetRebuildCandidate();
    g_rebuildCandidate.valid = 1;
    g_rebuildCandidate.map.map_valid = 1;
    g_rebuildCandidate.map.vram_x0 = PreviousPSXDisplay.DisplayPosition.x;
    g_rebuildCandidate.map.vram_y0 = PreviousPSXDisplay.DisplayPosition.y;
    g_rebuildCandidate.map.vram_x1 = PreviousPSXDisplay.DisplayEnd.x;
    g_rebuildCandidate.map.vram_y1 = PreviousPSXDisplay.DisplayEnd.y;
    g_rebuildCandidate.map.viewport_x0 = g_activeMap.viewport_x0;
    g_rebuildCandidate.map.viewport_y0 = g_activeMap.viewport_y0;
    g_rebuildCandidate.map.viewport_x1 = g_activeMap.viewport_x1;
    g_rebuildCandidate.map.viewport_y1 = g_activeMap.viewport_y1;
    g_rebuildCandidate.map.ratio = g_activeMap.ratio;
    g_rebuildCandidate.map.map_id = mapId;
    g_rebuildCandidate.map.rgb24 = 0;
    g_rebuildCandidate.map.content_valid = 1;
    g_rebuildCandidate.map.content_dirty = 1;
}

static int ActiveEfbHasFullTiles(void)
{
    int tx0, tx1, ty0, ty1, ty, tx;

    if (!g_activeMap.map_valid)
        return 0;
    if (g_activeMap.vram_x1 <= g_activeMap.vram_x0 ||
        g_activeMap.vram_y1 <= g_activeMap.vram_y0)
        return 0;

    tx0 = g_activeMap.vram_x0 >> 4;
    tx1 = (g_activeMap.vram_x1 - 1) >> 4;
    ty0 = g_activeMap.vram_y0 >> 4;
    ty1 = (g_activeMap.vram_y1 - 1) >> 4;

    for (ty = ty0; ty <= ty1; ty++)
        for (tx = tx0; tx <= tx1; tx++)
            if (g_efbTileState[ty & 31][tx & 63].cover == EFB_TILE_FULL)
                return 1;

    return 0;
}

static int CountEfbTiles(EfbTileCover cover)
{
    int ty, tx, count = 0;

    for (ty = 0; ty < VRAM_READBACK_TILE_Y; ty++)
        for (tx = 0; tx < VRAM_READBACK_TILE_X; tx++)
            if (g_efbTileState[ty][tx].cover == cover)
                count++;

    return count;
}

static int CountSnapshotTiles(const GXFrameSnapshot *snap,
                              EfbTileCover cover)
{
    int ty, tx, count = 0;

    if (!snap->valid)
        return 0;

    for (ty = 0; ty < VRAM_READBACK_TILE_Y; ty++)
        for (tx = 0; tx < VRAM_READBACK_TILE_X; tx++)
            if (snap->efbTile[ty][tx].cover == cover)
                count++;

    return count;
}

static int SnapshotOwnsTile(const GXFrameSnapshot *snap,
                            int px, int py)
{
    int tx = (px >> 4) & 63;
    int ty = (py >> 4) & 31;

    const EfbTileState *efb = &snap->efbTile[ty][tx];
    uint64_t cpu = g_psxWriteTileEpoch[ty][tx];

    if (efb->cover == EFB_TILE_FULL)
        return efb->seq != 0 && efb->seq >= cpu;

    return 0;
}

static int SnapshotCoversTile(const GXFrameSnapshot *snap, int tx, int ty)
{
    int px0 = tx << 4;
    int py0 = ty << 4;
    int px, py;

    if (snap == NULL || !snap->valid)
        return 0;

    for (py = py0; py < py0 + VRAM_READBACK_TILE_SIZE; py++)
        for (px = px0; px < px0 + VRAM_READBACK_TILE_SIZE; px++)
            if (!ContainsPixel(snap, px, py))
                return 0;

    return 1;
}

static int SnapshotExactRectCoversTile(const GXFrameSnapshot *snap,
                                       int tx, int ty)
{
    int px0 = tx << 4;
    int py0 = ty << 4;

    if (snap == NULL)
        return 0;
    if (!snap->exactRectValid)
        return 1;

    return snap->exact_x0 <= px0 &&
           snap->exact_y0 <= py0 &&
           snap->exact_x1 >= px0 + VRAM_READBACK_TILE_SIZE &&
           snap->exact_y1 >= py0 + VRAM_READBACK_TILE_SIZE;
}

/* T6-only color ownership: compares against the combined CPU/materialized
 * color epoch.  C0 helpers above keep comparing only the CPU epoch. */
static int SnapshotOwnsColorTileForBarrier(const GXFrameSnapshot *snap,
                                           int tx, int ty)
{
    const EfbTileState *efb;
    uint64_t psxColor;

    if (snap == NULL || !snap->valid)
        return 0;

    efb = &snap->efbTile[ty & 31][tx & 63];
    psxColor = PsxVuwColorTileEpoch(
        g_psxWriteTileEpoch[ty & 31][tx & 63],
        g_psxMaterializedColorTileEpoch[ty & 31][tx & 63]);

    return efb->cover == EFB_TILE_FULL && efb->seq != 0 &&
           efb->seq > psxColor;
}

static int ContainsPixel(const GXFrameSnapshot *snap, int px, int py)
{
    unsigned int relX, relY;

    if (snap->vram_w <= 0 || snap->vram_h <= 0)
        return 0;

    relX = (unsigned int)(px - snap->vram_x) & 1023u;
    relY = (unsigned int)(py - snap->vram_y) & 511u;

    return (int)relX < snap->vram_w && (int)relY < snap->vram_h;
}

static void EfbHazardsReset(void)
{
    g_efbContaminated  = FALSE;
    g_mixedMappingSeen = FALSE;
    g_untrackedEfbWrite = FALSE;
}

static void InvalidateSnapshotsForMap(uint32_t mapId)
{
    int i;

    for (i = 0; i < 2; i++)
        if (g_snapshot[i].valid && g_snapshot[i].map_id == mapId)
            g_snapshot[i].valid = 0;
}

static void SetEfbHazard(void)
{
    ResetActiveRebuildCandidate();
    InvalidateSnapshotsForMap(g_activeMap.map_id);
}

static void SetEfbMixedHazard(uint32_t otherMapId)
{
    ResetActiveRebuildCandidate();
    InvalidateSnapshotsForMap(g_activeMap.map_id);
    InvalidateSnapshotsForMap(otherMapId);
}

static void EfbDiscardedAfterPresent(void)
{
    ResetActiveRebuildCandidate();
    g_activeMap.content_valid = 0;
    g_activeMap.content_dirty = 0;
    memset(g_efbTileState, 0, sizeof(g_efbTileState));
    EfbHazardsReset();
}

static void EfbRebuiltFull(void)
{
    uint64_t seq = ++g_writeSeq;
    int x0, y0, x1, y1;

    ActiveMapVRamRect(&x0, &y0, &x1, &y1);
    MarkEfbTilesFull(seq, x0, y0, x1, y1);
    g_activeMap.content_valid = 1;
    g_activeMap.content_dirty = 1;
    g_activeMap.contentWriteSeq = seq;
    EfbHazardsReset();
}

static void OnEfbDrawSubmitted(void)
{
    EfbDrawContext *ctx;
    uint64_t seq;

    g_debugDrawSubmitted++;

    if (!ReadbackEnabled())
        return;

    if (g_efbOverflowDepth > 0)
    {
        g_untrackedEfbWrite = TRUE;
        SetEfbHazard();
        g_activeMap.content_valid = 1;
        g_activeMap.content_dirty = 1;
        g_activeMap.contentWriteSeq = ++g_writeSeq;
        return;
    }

    ctx = TopEfbContext();

    if (ctx == NULL)
    {
        g_untrackedEfbWrite = TRUE;
        SetEfbHazard();
        g_activeMap.content_valid = 1;
        g_activeMap.content_dirty = 1;
        g_activeMap.contentWriteSeq = ++g_writeSeq;
        return;
    }

    if (!ctx->externalRebuild && !ctx->rebuildUpload)
        ResetActiveRebuildCandidate();

    if (!ctx->active)
        return;

    if (!ctx->rectSet)
    {
        g_untrackedEfbWrite = TRUE;
        SetEfbHazard();
        g_activeMap.content_valid = 1;
        g_activeMap.content_dirty = 1;
        g_activeMap.contentWriteSeq = ++g_writeSeq;
        return;
    }

    seq = ctx->fixedSeq ? ctx->fixedSeq : ++g_writeSeq;

    if (ctx->externalRebuild)
    {
        ResetActiveRebuildCandidate();
#ifdef DISP_DEBUG
        if (g_diagLastActiveWriteSeq != 0 &&
            g_diagLastActiveWriteSeq != g_diagLastExternalLogSeq)
        {
            sprintf(txtbuffer,
                    "VRD EXTOVER cycle=%u activeSeq=%u uploadSeq=%u "
                    "activeMap=%u externalMap=%u rect=%d,%d-%d,%d\r\n",
                    g_diagCycle,
                    (unsigned int)g_diagLastActiveWriteSeq,
                    (unsigned int)seq,
                    g_activeMap.map_id, ctx->mapId,
                    ctx->x0, ctx->y0, ctx->x1, ctx->y1);
            writeLogFile(txtbuffer);
            g_diagLastExternalLogSeq = g_diagLastActiveWriteSeq;
        }
#endif
        if (!g_rebuildCandidate.valid ||
            ctx->mapId != g_rebuildCandidate.map.map_id)
        {
            ResetRebuildCandidate();
            return;
        }

        ctx->lastSeq = seq;
        ctx->submittedAny = 1;
        return;
    }

    if (ctx->pendingRender)
    {
        if (g_rebuildCandidate.valid)
            ResetRebuildCandidate();

        if (ctx->coverage == EFB_TILE_FULL)
            MarkTileArrayFull(g_pendingPresentedTile, seq,
                              ctx->x0, ctx->y0, ctx->x1, ctx->y1);
        else
            MarkTileArrayPartial(g_pendingPresentedTile, seq,
                                 ctx->x0, ctx->y0, ctx->x1, ctx->y1);

        g_pendingPresentedMap.content_valid = 1;
        g_pendingPresentedMap.content_dirty = 1;
        g_pendingPresentedMap.contentWriteSeq = seq;
        ctx->lastSeq = seq;
        ctx->submittedAny = 1;
        return;
    }

    if (g_rebuildCandidate.valid)
        ResetRebuildCandidate();

    if (ctx->coverage == EFB_TILE_FULL)
    {
        MarkEfbTilesFull(seq, ctx->x0, ctx->y0, ctx->x1, ctx->y1);
        if (ctx->presentationRebuild)
        {
            g_pendingPresentedMap = g_activeMap;
            g_pendingPresentedMap.map_valid = 1;
            g_pendingPresentedMap.content_valid = 1;
            g_pendingPresentedMap.content_dirty = 1;
            g_pendingPresentedMap.vram_x0 = ctx->x0;
            g_pendingPresentedMap.vram_y0 = ctx->y0;
            g_pendingPresentedMap.vram_x1 = ctx->x1;
            g_pendingPresentedMap.vram_y1 = ctx->y1;
            g_pendingPresentedMap.map_id = INVALID_MAP_ID;
            g_pendingPresentedMap.contentWriteSeq = seq;
            g_pendingPresentedReady = 1;
            memset(g_pendingPresentedTile, 0,
                   sizeof(g_pendingPresentedTile));
            MarkTileArrayFull(g_pendingPresentedTile, seq,
                              ctx->x0, ctx->y0, ctx->x1, ctx->y1);
            EfbHazardsReset();
        }
        else if (ctx->mapId == g_activeMap.map_id &&
                 ctx->x0 <= g_activeMap.vram_x0 &&
                 ctx->y0 <= g_activeMap.vram_y0 &&
                 ctx->x1 >= g_activeMap.vram_x1 &&
                 ctx->y1 >= g_activeMap.vram_y1)
            EfbHazardsReset();
    }
    else
        MarkEfbTilesPartial(seq, ctx->x0, ctx->y0, ctx->x1, ctx->y1);

    g_activeMap.content_valid = 1;
    g_activeMap.content_dirty = 1;
    g_activeMap.contentWriteSeq = seq;
#ifdef DISP_DEBUG
    g_diagLastActiveWriteSeq = seq;
#endif
    ctx->lastSeq = seq;

    if (ctx->mapId != g_activeMap.map_id)
    {
        g_mixedMappingSeen = TRUE;
        SetEfbMixedHazard(ctx->mapId);
    }

    ctx->submittedAny = 1;
}

static void OnEfbClearSubmitted(void)
{
    EfbDrawContext *ctx;
    uint64_t seq;
    int x0, y0, x1, y1;

    if (!ReadbackEnabled())
        return;

    ResetActiveRebuildCandidate();

    if (g_efbOverflowDepth > 0)
    {
        g_untrackedEfbWrite = TRUE;
        SetEfbHazard();
        g_activeMap.content_valid = 1;
        g_activeMap.content_dirty = 1;
        g_activeMap.contentWriteSeq = ++g_writeSeq;
        return;
    }

    ctx = TopEfbContext();
    if (ctx == NULL || ctx->fixedSeq == 0)
    {
        g_untrackedEfbWrite = TRUE;
        SetEfbHazard();
        g_activeMap.content_valid = 1;
        g_activeMap.content_dirty = 1;
        g_activeMap.contentWriteSeq = ++g_writeSeq;
        return;
    }

    seq = ctx->fixedSeq;
    ActiveMapVRamRect(&x0, &y0, &x1, &y1);
    MarkEfbTilesFull(seq, x0, y0, x1, y1);
    g_activeMap.content_valid = 1;
    g_activeMap.content_dirty = 1;
    g_activeMap.contentWriteSeq = seq;
    EfbHazardsReset();
}

static uint32_t ResolveUploadMapId(int Position)
{
    if (Position == -1)
        return INVALID_MAP_ID;

    if (Position == TRUE)
        return g_activeMap.map_id;

    /* UploadScreen(FALSE) uses PreviousPSXDisplay for coordinate offsetting,
     * but current and previous display rectangles are commonly identical.
     * Treat that case as the active mapping instead of manufacturing a mixed
     * mapping hazard from a stale map id. */
    if (g_activeMap.map_valid &&
        PreviousPSXDisplay.DisplayPosition.x == g_activeMap.vram_x0 &&
        PreviousPSXDisplay.DisplayPosition.y == g_activeMap.vram_y0 &&
        PreviousPSXDisplay.DisplayEnd.x == g_activeMap.vram_x1 &&
        PreviousPSXDisplay.DisplayEnd.y == g_activeMap.vram_y1)
        return g_activeMap.map_id;

    return g_prevMapId;
}

static int AllocSnapshotBuffer(GXFrameSnapshot *snap, int size)
{
    void *buf = _mem2_malloc(size);

    if (!buf)
        return 0;

    if (snap->pixels)
        _mem2_free(snap->pixels);

    snap->pixels = buf;
    snap->size = size;
    snap->valid = 0;
    return 1;
}

static BOOL CanCaptureActiveEfb(void)
{
    if (!ReadbackEnabled())
        return FALSE;
    if (STATUSREG & GPUSTATUS_RGB24)
        return FALSE;
    if (PSXDisplay.RGB24)
        return FALSE;
    if (g_activeMap.vram_x1 <= g_activeMap.vram_x0 ||
        g_activeMap.vram_y1 <= g_activeMap.vram_y0 ||
        g_activeMap.viewport_x1 <= g_activeMap.viewport_x0 ||
        g_activeMap.viewport_y1 <= g_activeMap.viewport_y0)
        return FALSE;

    return g_activeMap.map_valid &&
           g_activeMap.content_valid &&
           !g_efbContaminated &&
           !g_mixedMappingSeen &&
           !g_untrackedEfbWrite;
}

static int CaptureEfbSnapshotWithTiles(
    GXFrameSnapshot *snap, const GXDisplayMap *map,
    EfbTileState tile[VRAM_READBACK_TILE_Y][VRAM_READBACK_TILE_X],
    SnapshotSource source)
{
    int copyW = (iResX + 15) & ~15;
    int copyH = (iResY + 1) & ~1;
    int size  = GX_GetTexBufferSize(copyW, copyH,
                                    GX_TF_RGB5A3, 0, GX_FALSE);

    if (!map->content_valid)
        return 0;

    if (snap->pixels == NULL || snap->size < size)
    {
        if (!AllocSnapshotBuffer(snap, size))
            return 0;
    }

    GX_SetCopyFilter(vmode ? vmode->aa : GX_FALSE,
                     (vmode && vmode->aa) ? vmode->sample_pattern : NULL,
                     GX_FALSE, NULL);
    GX_SetTexCopySrc(0, 0, iResX, iResY);
    GX_SetTexCopyDst(copyW, copyH, GX_TF_RGB5A3, GX_FALSE);
    GX_CopyTex(MEM_K0_TO_K1(snap->pixels), GX_FALSE);
    GX_PixModeSync();
    GX_DrawDone();
    DCInvalidateRange(snap->pixels, size);

    snap->valid = 1;
    snap->vram_x = map->vram_x0;
    snap->vram_y = map->vram_y0;
    snap->vram_w = map->vram_x1 - map->vram_x0;
    snap->vram_h = map->vram_y1 - map->vram_y0;
    snap->viewport_x = map->viewport_x0;
    snap->viewport_y = map->viewport_y0;
    snap->viewport_w = map->viewport_x1 - map->viewport_x0;
    snap->viewport_h = map->viewport_y1 - map->viewport_y0;
    snap->ratio = map->ratio;
    snap->map_id = map->map_id;
    snap->source = source;
    snap->exactRectValid = 0;

    snap->capturedContentSeq = map->contentWriteSeq;
    snap->captureOrder = ++g_captureOrder;
    memcpy(snap->efbTile, tile, sizeof(snap->efbTile));

    snap->size = size;
    snap->texWidth = copyW;
    snap->texHeight = copyH;

    RestoreDispCopyInfo();
    return 1;
}

static int CaptureEfbSnapshot(GXFrameSnapshot *snap,
                              const GXDisplayMap *map,
                              SnapshotSource source)
{
    if (!ActiveEfbHasFullTiles())
        return 0;

    return CaptureEfbSnapshotWithTiles(snap, map, g_efbTileState, source);
}

static void EstablishRebuildBaseline(
    const GXDisplayMap *map,
    EfbTileState tile[VRAM_READBACK_TILE_Y][VRAM_READBACK_TILE_X],
    SnapshotSource source)
{
    uint64_t savedCaptureOrder = g_captureOrder;

    if (!CaptureEfbSnapshotWithTiles(&g_rebuildBaseline, map, tile, source))
        g_rebuildBaseline.valid = 0;

    /* The baseline is not a selectable timeline snapshot. */
    g_captureOrder = savedCaptureOrder;
}

/* Active-map rebuild provenance.  Only UploadScreen chunks of a full-map CPU
 * rebuild write this candidate; g_efbTileState also records GX/clear sources
 * and must never be used to prove CPU ownership. */
static void MarkActiveRebuildTileUpload(uint64_t seq,
                                        int x0, int y0, int x1, int y1)
{
    if (!ReadbackEnabled() || !g_activeMap.map_valid || seq == 0)
        return;

    if (!g_activeRebuildCandidate.valid)
    {
        g_activeRebuildCandidate.valid = 1;
        g_activeRebuildCandidate.map = g_activeMap;
        g_activeRebuildCandidate.map.content_valid = 1;
        g_activeRebuildCandidate.map.content_dirty = 1;
        g_activeRebuildCandidate.map.contentWriteSeq = 0;
    }

    if (g_activeRebuildCandidate.complete ||
        g_activeRebuildCandidate.map.map_id != g_activeMap.map_id)
        return;

    MarkTileArrayFull(g_activeRebuildCandidate.tile, seq, x0, y0, x1, y1);
    g_activeRebuildCandidate.map.contentWriteSeq = seq;
}

/* Establish the single baseline only when the upload provenance candidate
 * itself becomes complete.  A mixed-source g_efbTileState never qualifies. */
static void MaybeEstablishActiveRebuildBaseline(void)
{
    if (!ReadbackEnabled() ||
        !T6RebuildCandidateEstablishes(
            g_activeRebuildCandidate.valid,
            g_activeRebuildCandidate.complete,
            TileArrayCoversMap(g_activeRebuildCandidate.tile,
                               &g_activeRebuildCandidate.map)))
        return;

    g_activeRebuildCandidate.complete = 1;
    EstablishRebuildBaseline(&g_activeRebuildCandidate.map,
                             g_activeRebuildCandidate.tile,
                             SNAPSHOT_SOURCE_UPLOAD_REBUILD);
}

/* Full-screen BlkFill writes EFB and psxVuw with the same fixed sequence and
 * explicitly covers the whole active map, so it is its own CPU-authoritative
 * rebuild source independent of the upload candidate. */
static void MaybeEstablishBlkFillBaseline(void)
{
    if (!ReadbackEnabled() || !g_activeMap.map_valid ||
        !TileArrayCoversMap(g_efbTileState, &g_activeMap))
        return;

    if (g_rebuildBaseline.valid &&
        g_rebuildBaseline.map_id == g_activeMap.map_id &&
        g_rebuildBaseline.capturedContentSeq == g_activeMap.contentWriteSeq)
        return;

    EstablishRebuildBaseline(&g_activeMap, g_efbTileState,
                             SNAPSHOT_SOURCE_UPLOAD_REBUILD);
}

static GXFrameSnapshot *OldestSnapshotSlot(void)
{
    GXFrameSnapshot *live = LIVE_SNAP();
    GXFrameSnapshot *prev = PREV_SNAP();

    if (!live->valid)
        return live;
    if (!prev->valid)
        return prev;
    return live->captureOrder <= prev->captureOrder ? live : prev;
}

static int RectOverlapArea(int ax0, int ay0, int ax1, int ay1,
                           int bx0, int by0, int bx1, int by1)
{
    int x0 = max(ax0, bx0);
    int y0 = max(ay0, by0);
    int x1 = min(ax1, bx1);
    int y1 = min(ay1, by1);

    return (x1 > x0 && y1 > y0) ? (x1 - x0) * (y1 - y0) : 0;
}

/* Vagrant Story keeps CopyDisp's EFB contents and renders the alternate
 * 320-pixel VRAM page by changing DrawArea/DrawOffset before issuing C0.
 * GP1 still names the other page, so the generic active-map tracker cannot
 * assign those draws ownership.  Recover only the small, fully-contained C0
 * rectangle when DrawArea unambiguously identifies the previous page. */
static int TryCapturePreviousReadRect(int x, int y, int w, int h)
{
    GXDisplayMap map;
    GXFrameSnapshot *slot;
    uint64_t seq;
    int drawX0, drawY0, drawX1, drawY1;
    int previousOverlap, activeOverlap;
    int tx0, tx1, ty0, ty1, tx, ty;

    if (!(dwActFixes & AUTO_FIX_NO_SWAP_BUF) ||
        !ReadbackEnabled() || w <= 0 || h <= 0 ||
        w > 64 || w * h > 4096 ||
        (STATUSREG & GPUSTATUS_RGB24) || PSXDisplay.RGB24 ||
        g_prevMapId == INVALID_MAP_ID || !g_activeMap.map_valid ||
        !g_activeMap.content_valid || !g_activeMap.content_dirty ||
        g_efbContaminated || g_mixedMappingSeen || g_untrackedEfbWrite)
        return 0;

    /* The fallback deliberately excludes wrapping C0 rectangles. */
    if (x < PreviousPSXDisplay.DisplayPosition.x ||
        y < PreviousPSXDisplay.DisplayPosition.y ||
        x + w > PreviousPSXDisplay.DisplayEnd.x ||
        y + h > PreviousPSXDisplay.DisplayEnd.y)
        return 0;

    drawX0 = PSXDisplay.DrawArea.x0;
    drawY0 = PSXDisplay.DrawArea.y0;
    drawX1 = PSXDisplay.DrawArea.x1 + 1;
    drawY1 = PSXDisplay.DrawArea.y1 + 1;
    previousOverlap = RectOverlapArea(
        drawX0, drawY0, drawX1, drawY1,
        PreviousPSXDisplay.DisplayPosition.x,
        PreviousPSXDisplay.DisplayPosition.y,
        PreviousPSXDisplay.DisplayEnd.x,
        PreviousPSXDisplay.DisplayEnd.y);
    activeOverlap = RectOverlapArea(
        drawX0, drawY0, drawX1, drawY1,
        g_activeMap.vram_x0, g_activeMap.vram_y0,
        g_activeMap.vram_x1, g_activeMap.vram_y1);
    if (previousOverlap == 0 || previousOverlap <= activeOverlap)
        return 0;

    seq = g_activeMap.contentWriteSeq;
    if (seq == 0)
        return 0;

    map = g_activeMap;
    map.vram_x0 = PreviousPSXDisplay.DisplayPosition.x;
    map.vram_y0 = PreviousPSXDisplay.DisplayPosition.y;
    map.vram_x1 = PreviousPSXDisplay.DisplayEnd.x;
    map.vram_y1 = PreviousPSXDisplay.DisplayEnd.y;
    map.map_id = g_prevMapId;
    map.map_valid = 1;
    map.content_valid = 1;
    map.content_dirty = 1;
    map.contentWriteSeq = seq;

    slot = OldestSnapshotSlot();
    if (!CaptureEfbSnapshotWithTiles(slot, &map, g_efbTileState,
                                     SNAPSHOT_SOURCE_PRESENTED))
        return 0;

    memset(slot->efbTile, 0, sizeof(slot->efbTile));
    tx0 = x >> 4;
    tx1 = (x + w - 1) >> 4;
    ty0 = y >> 4;
    ty1 = (y + h - 1) >> 4;
    for (ty = ty0; ty <= ty1; ty++)
        for (tx = tx0; tx <= tx1; tx++)
        {
            slot->efbTile[ty & 31][tx & 63].cover = EFB_TILE_FULL;
            slot->efbTile[ty & 31][tx & 63].seq = seq;
        }

    slot->exactRectValid = 1;
    slot->exact_x0 = x;
    slot->exact_y0 = y;
    slot->exact_x1 = x + w;
    slot->exact_y1 = y + h;

    return 1;
}

static int CapturePresentedEfbSnapshot(void)
{
    GXDisplayMap map;
    GXFrameSnapshot *slot;

    if (g_pendingPresentedReady)
    {
        if (!ReadbackEnabled() ||
            (STATUSREG & GPUSTATUS_RGB24) || PSXDisplay.RGB24 ||
            g_efbContaminated || g_mixedMappingSeen ||
            g_untrackedEfbWrite)
            return 0;
        map = g_pendingPresentedMap;
    }
    else
    {
        if (!CanCaptureActiveEfb() || !g_activeMap.content_dirty)
            return 0;
        map = g_activeMap;
    }

    slot = OldestSnapshotSlot();
    if (g_pendingPresentedReady)
    {
        if (g_pendingPresentedMap.vram_x0 != g_activeMap.vram_x0 ||
            g_pendingPresentedMap.vram_y0 != g_activeMap.vram_y0 ||
            g_pendingPresentedMap.vram_x1 != g_activeMap.vram_x1 ||
            g_pendingPresentedMap.vram_y1 != g_activeMap.vram_y1)
        {
            if (!CaptureEfbSnapshotWithTiles(slot, &map,
                                             g_pendingPresentedTile,
                                             SNAPSHOT_SOURCE_PRESENTED))
                return 0;
        }
        else if (!CaptureEfbSnapshotWithTiles(slot, &map,
                                              g_efbTileState,
                                              SNAPSHOT_SOURCE_PRESENTED))
        {
            return 0;
        }
    }
    else if (!CaptureEfbSnapshot(slot, &map,
                                 SNAPSHOT_SOURCE_PRESENTED))
    {
        return 0;
    }

#ifdef DISP_DEBUG
    DebugComparePresentedToBaseline(slot);
#endif

    if (g_pendingPresentedReady)
    {
        g_pendingPresentedCaptureOrder = slot->captureOrder;
        g_pendingPresentedReady = 0;
    }
    return 1;
}

static int FinishPreviousRebuildChunk(uint64_t seq,
                                      int x0, int y0, int x1, int y1)
{
    if (!g_rebuildCandidate.valid || seq == 0)
        return 0;

    if (g_rebuildCandidate.complete)
        return 0;

    MarkTileArrayFull(g_rebuildCandidate.tile, seq, x0, y0, x1, y1);
    g_rebuildCandidate.map.contentWriteSeq = seq;

    if (!TileArrayCoversMap(g_rebuildCandidate.tile,
                            &g_rebuildCandidate.map))
        return 0;

    g_rebuildCandidate.complete = 1;
    if (g_pendingPresentedReady &&
        g_pendingPresentedMap.vram_x0 == g_rebuildCandidate.map.vram_x0 &&
        g_pendingPresentedMap.vram_y0 == g_rebuildCandidate.map.vram_y0 &&
        g_pendingPresentedMap.vram_x1 == g_rebuildCandidate.map.vram_x1 &&
        g_pendingPresentedMap.vram_y1 == g_rebuildCandidate.map.vram_y1)
    {
        memcpy(g_pendingPresentedTile, g_rebuildCandidate.tile,
               sizeof(g_pendingPresentedTile));
        g_pendingPresentedMap.contentWriteSeq = seq;
    }

    EstablishRebuildBaseline(&g_rebuildCandidate.map,
                             g_rebuildCandidate.tile,
                             SNAPSHOT_SOURCE_UPLOAD_REBUILD);
    return 1;
}

static int ResolveCompletedRebuildForRead(int x, int y, int w, int h)
{
    uint32_t mapId;

    if (!g_rebuildCandidate.valid || !g_rebuildCandidate.complete)
        return 0;

    /* Keep this path conservative: the reconstructed image must exactly be
     * the VRAM rectangle requested by C0. */
    if (x != g_rebuildCandidate.map.vram_x0 ||
        y != g_rebuildCandidate.map.vram_y0 ||
        w != g_rebuildCandidate.map.vram_x1 -
             g_rebuildCandidate.map.vram_x0 ||
        h != g_rebuildCandidate.map.vram_y1 -
             g_rebuildCandidate.map.vram_y0)
        return 0;

    /* This EFB image was produced exclusively from A0 data which is already
     * exact in psxVuw.  Copying it back through RGB5A3 is both redundant and
     * lossy.  Consume the reconstruction marker and let CPU tile epochs reject
     * older presented snapshots; a later real GX draw follows the normal live
     * or presented snapshot path. */
    mapId = g_rebuildCandidate.map.map_id;

#ifdef DISP_DEBUG
    {
        uint64_t savedCaptureOrder = g_captureOrder;

        if (CaptureEfbSnapshotWithTiles(&g_diagBaseline,
                                        &g_rebuildCandidate.map,
                                        g_rebuildCandidate.tile,
                                        SNAPSHOT_SOURCE_UPLOAD_REBUILD))
        {
            g_diagCycle++;
            g_diagBaselineDrawCount = g_debugDrawSubmitted;
            g_diagBaselinePending = 1;
            sprintf(txtbuffer,
                    "VRD BASE cycle=%u map=%u rect=%d,%d %dx%d "
                    "efb=%dx%d draws=%u\r\n",
                    g_diagCycle, mapId, x, y, w, h,
                    g_diagBaseline.texWidth,
                    g_diagBaseline.texHeight,
                    g_diagBaselineDrawCount);
            writeLogFile(txtbuffer);
        }
        else
        {
            sprintf(txtbuffer,
                    "VRD BASE cycle=%u capture=FAIL map=%u "
                    "rect=%d,%d %dx%d\r\n",
                    g_diagCycle + 1, mapId, x, y, w, h);
            writeLogFile(txtbuffer);
        }
        g_captureOrder = savedCaptureOrder;
    }
    sprintf(txtbuffer,
            "VRB REBUILD cpu-authoritative map=%u rect=%d,%d %dx%d\r\n",
            mapId, x, y, w, h);
    writeLogFile(txtbuffer);
#endif

    ResetRebuildCandidate();
    return 1;
}

static void RetireLiveSnapshotToPrev(void)
{
    GXFrameSnapshot *live = LIVE_SNAP();
    GXFrameSnapshot *prev = PREV_SNAP();

    if (!live->valid)
        return;

    if (!prev->valid || live->captureOrder > prev->captureOrder)
    {
        int tmp = g_liveSlot;
        g_liveSlot = g_prevSlot;
        g_prevSlot = tmp;
        LIVE_SNAP()->valid = 0;
    }
}

static void TryCaptureLiveFrame(void)
{
    g_lastCaptureResult = 0;

    if (!ReadbackEnabled())
    {
        g_lastCaptureResult = -1;
        return;
    }
    if (!CanCaptureActiveEfb())
    {
        g_lastCaptureResult = -2;
        return;
    }
    if (!g_activeMap.content_dirty)
    {
        g_lastCaptureResult = -3;
        return;
    }

    if (LIVE_SNAP()->valid &&
        LIVE_SNAP()->map_id != g_activeMap.map_id)
        RetireLiveSnapshotToPrev();

    if (CaptureEfbSnapshot(LIVE_SNAP(), &g_activeMap,
                           SNAPSHOT_SOURCE_CURRENT_EFB))
    {
        g_activeMap.content_dirty = 0;
        g_lastCaptureResult = 1;
    }
    else
    {
        g_lastCaptureResult = -4;
    }
}

static int DisplayMapEquals(const GXDisplayMap *a, const GXDisplayMap *b)
{
    return a->vram_x0 == b->vram_x0 &&
           a->vram_y0 == b->vram_y0 &&
           a->vram_x1 == b->vram_x1 &&
           a->vram_y1 == b->vram_y1 &&
           a->viewport_x0 == b->viewport_x0 &&
           a->viewport_y0 == b->viewport_y0 &&
           a->viewport_x1 == b->viewport_x1 &&
           a->viewport_y1 == b->viewport_y1 &&
           a->ratio.left == b->ratio.left &&
           a->ratio.top == b->ratio.top &&
           a->ratio.right == b->ratio.right &&
           a->ratio.bottom == b->ratio.bottom &&
           a->rgb24 == b->rgb24;
}

static void GetProposedActiveMap(GXDisplayMap *out)
{
    memset(out, 0, sizeof(*out));

    out->vram_x0 = PSXDisplay.DisplayPosition.x;
    out->vram_y0 = PSXDisplay.DisplayPosition.y;
    out->vram_x1 = PSXDisplay.DisplayEnd.x;
    out->vram_y1 = PSXDisplay.DisplayEnd.y;

    out->viewport_x0 = rRatioRect.left;
    out->viewport_y0 = iResY - (rRatioRect.top + rRatioRect.bottom);
    out->viewport_x1 = rRatioRect.left + rRatioRect.right;
    out->viewport_y1 = out->viewport_y0 + rRatioRect.bottom;
    out->ratio = rRatioRect;
    out->rgb24 = PSXDisplay.RGB24;
}

static void BuildActiveMapFromDisplay(void)
{
    GXDisplayMap next;
    int i;
    GetProposedActiveMap(&next);

    if (g_activeMap.map_valid && DisplayMapEquals(&g_activeMap, &next))
        return;

    g_activeMap = next;
    g_activeMap.map_id = ++g_mapId;
    g_activeMap.map_valid = 1;
    g_activeMap.content_valid = 0;
    g_activeMap.content_dirty = 0;
    g_activeMap.contentWriteSeq = 0;

    if (g_pendingPresentedCaptureOrder != 0)
    {
        for (i = 0; i < 2; i++)
        {
            GXFrameSnapshot *snap = &g_snapshot[i];

            if (!snap->valid ||
                snap->captureOrder != g_pendingPresentedCaptureOrder)
                continue;

            if (snap->source == SNAPSHOT_SOURCE_PRESENTED &&
                snap->map_id == INVALID_MAP_ID &&
                snap->vram_x == g_activeMap.vram_x0 &&
                snap->vram_y == g_activeMap.vram_y0 &&
                snap->vram_w == g_activeMap.vram_x1 - g_activeMap.vram_x0 &&
                snap->vram_h == g_activeMap.vram_y1 - g_activeMap.vram_y0)
                snap->map_id = g_activeMap.map_id;
            else
                snap->valid = 0;
            break;
        }
        g_pendingPresentedCaptureOrder = 0;
    }

    memset(g_efbTileState, 0, sizeof(g_efbTileState));
    ResetRebuildCandidate();
    ResetActiveRebuildCandidate();
}

static int OnDisplayMappingWillChange(const GXDisplayMap *proposed)
{
    if (proposed != NULL &&
        g_activeMap.map_valid &&
        DisplayMapEquals(&g_activeMap, proposed))
        return 0;

    if (g_displayMapTxDepth++ > 0)
        return 1;

    if (CanCaptureActiveEfb() && g_activeMap.content_dirty)
        CapturePresentedEfbSnapshot();

    g_prevMapId = g_activeMap.map_id;
    g_displayMapTxChanged = FALSE;
    return 1;
}

static void OnDisplayMappingChanged(void)
{
    if (g_displayMapTxDepth == 0)
        return;

    g_displayMapTxChanged = TRUE;
    g_displayMapTxDepth--;

    if (g_displayMapTxDepth == 0 && g_displayMapTxChanged)
        BuildActiveMapFromDisplay();
}

static void ResetVramReadbackState(void)
{
    int i;

#if T6_BARRIER_DIAG
    sprintf(txtbuffer,
            "TRB STATS calls=%u noAction=%u materialized=%u unresolved=%u "
            "reentrant=%u copyTexTotal=%u changed=%u invalRuns=%u\r\n",
            g_vramReadBarrierCalls, g_vramReadBarrierNoAction,
            g_vramReadBarrierMaterialized, g_vramReadBarrierUnresolved,
            g_vramReadBarrierReentrant, g_vramReadBarrierCopyTexTotal,
            g_vramReadBarrierChangedTiles,
            g_vramReadBarrierInvalidateRuns);
    writeLogFile(txtbuffer);
#endif

    g_writeSeq = 0;
    g_captureOrder = 0;
    g_mapId = 0;
    g_prevMapId = 0;
    g_liveSlot = 0;
    g_prevSlot = 1;
    g_efbOverflowDepth = 0;
    g_displayMapTxDepth = 0;
    g_displayMapTxChanged = FALSE;
    g_pendingPresentedReady = 0;
    g_pendingPresentedCaptureOrder = 0;
    g_vramReadBarrierDepth = 0;
    g_t6WorkspaceDepth = 0;
#ifdef DISP_DEBUG
    g_debugDrawSubmitted = 0;
    g_diagCycle = 0;
    g_diagBaselineDrawCount = 0;
    g_diagBaselinePending = 0;
    g_diagLastActiveWriteSeq = 0;
    g_diagLastExternalLogSeq = 0;
#if T6_BARRIER_DIAG
    g_vramReadBarrierCalls = 0;
    g_vramReadBarrierNoAction = 0;
    g_vramReadBarrierMaterialized = 0;
    g_vramReadBarrierUnresolved = 0;
    g_vramReadBarrierReentrant = 0;
    g_vramReadBarrierLastReason = T6_REASON_NONE;
    T6CopyTexBeginCall(&g_vramReadBarrierCopyTex);
    g_vramReadBarrierCopyTexTotal = 0;
    g_vramReadBarrierChangedTiles = 0;
    g_vramReadBarrierInvalidateRuns = 0;
    g_vramReadBarrierSuccessLogs = 0;
    memset(g_vramReadBarrierReasonLogs, 0,
           sizeof(g_vramReadBarrierReasonLogs));
#endif
#endif

    memset(g_psxWriteTileEpoch, 0, sizeof(g_psxWriteTileEpoch));
    memset(g_psxMaterializedColorTileEpoch, 0,
           sizeof(g_psxMaterializedColorTileEpoch));
    memset(g_efbTileState, 0, sizeof(g_efbTileState));
    memset(&g_pendingPresentedMap, 0, sizeof(g_pendingPresentedMap));
    memset(g_pendingPresentedTile, 0, sizeof(g_pendingPresentedTile));
    ResetRebuildCandidate();
    ResetActiveRebuildCandidate();

    g_efbContaminated  = FALSE;
    g_mixedMappingSeen = FALSE;
    g_untrackedEfbWrite = FALSE;

    g_efbContextDepth = 0;
    memset(g_efbContextStack, 0, sizeof(g_efbContextStack));

    g_readbackState = READBACK_IDLE;
    g_lastCaptureResult = 0;
    g_lastReadMapping = MAPPING_UNKNOWN;
#ifdef DISP_DEBUG
    g_lastMergedPixels = 0;
    g_lastMergedChangedPixels = 0;
    g_lastMergedMaskOnlyPixels = 0;
    g_lastMergedRgbChangedPixels = 0;
    g_lastMergeOldMaskPixels = 0;
    g_lastMergeNewMaskPixels = 0;
    g_lastMergeOldHash = 2166136261u;
    g_lastMergeNewHash = 2166136261u;
    g_lastMergedCurrentPixels = 0;
    g_lastMergedPresentedPixels = 0;
    g_lastMergedRebuildPixels = 0;
#endif

    g_activeMap.map_valid = 0;
    g_activeMap.content_valid = 0;
    g_activeMap.content_dirty = 0;

    for (i = 0; i < 2; i++)
    {
        if (g_snapshot[i].pixels)
            _mem2_free(g_snapshot[i].pixels);
        g_snapshot[i].pixels = NULL;
        g_snapshot[i].valid  = 0;
        g_snapshot[i].size   = 0;
    }

    if (g_rebuildBaseline.pixels)
        _mem2_free(g_rebuildBaseline.pixels);
    memset(&g_rebuildBaseline, 0, sizeof(g_rebuildBaseline));

#ifdef DISP_DEBUG
    if (g_diagBaseline.pixels)
        _mem2_free(g_diagBaseline.pixels);
    memset(&g_diagBaseline, 0, sizeof(g_diagBaseline));
#endif
}

static int ReadSnapshotPixelRaw(const GXFrameSnapshot *snap,
                                int px, int py, uint16_t *value)
{
    int nx = (int)((unsigned int)(px - snap->vram_x) & 1023u);
    int ny = (int)((unsigned int)(py - snap->vram_y) & 511u);
    int tx, ty;
    unsigned short gx;

    if (nx >= snap->vram_w || ny >= snap->vram_h)
        return 0;

    tx = snap->viewport_x +
         (nx * snap->viewport_w) / snap->vram_w;
    ty = snap->viewport_y +
         (ny * snap->viewport_h) / snap->vram_h;

    if (tx < 0 || tx >= snap->texWidth ||
        ty < 0 || ty >= snap->texHeight)
        return 0;

    gx = ReadGXRGB5A3PixelRaw(snap->pixels, snap->texWidth, tx, ty);
    *value = gx;
    return 1;
}

static uint16_t ReadSnapshotPixelToPSX(const GXFrameSnapshot *snap,
                                       int px, int py)
{
    uint16_t gx;

    if (!ReadSnapshotPixelRaw(snap, px, py, &gx))
        return 0;

    return GXRGB5A3ToPSX15(gx);
}

#ifdef DISP_DEBUG
static void DebugComparePresentedToBaseline(const GXFrameSnapshot *snap)
{
    unsigned int quadrant[4] = {0, 0, 0, 0};
    unsigned int baseHash = 2166136261u;
    unsigned int presentHash = 2166136261u;
    unsigned int diff = 0;
    int minX = iResX, minY = iResY, maxX = -1, maxY = -1;
    int sameRect;
    int width, height, x, y;

    if (!g_diagBaselinePending || !g_diagBaseline.valid || !snap->valid)
        return;

    sameRect =
        g_diagBaseline.vram_x == snap->vram_x &&
        g_diagBaseline.vram_y == snap->vram_y &&
        g_diagBaseline.vram_w == snap->vram_w &&
        g_diagBaseline.vram_h == snap->vram_h;
    if (!sameRect)
    {
        sprintf(txtbuffer,
                "VRD PRESENT cycle=%u match=0 baseMap=%u presentMap=%u "
                "baseRect=%d,%d %dx%d presentRect=%d,%d %dx%d\r\n",
                g_diagCycle, g_diagBaseline.map_id, snap->map_id,
                g_diagBaseline.vram_x, g_diagBaseline.vram_y,
                g_diagBaseline.vram_w, g_diagBaseline.vram_h,
                snap->vram_x, snap->vram_y,
                snap->vram_w, snap->vram_h);
        writeLogFile(txtbuffer);
        return;
    }

    width = min(iResX, min(g_diagBaseline.texWidth, snap->texWidth));
    height = min(iResY, min(g_diagBaseline.texHeight, snap->texHeight));
    for (y = 0; y < height; y++)
    {
        for (x = 0; x < width; x++)
        {
            unsigned short base = ReadGXRGB5A3PixelRaw(
                g_diagBaseline.pixels, g_diagBaseline.texWidth, x, y);
            unsigned short present = ReadGXRGB5A3PixelRaw(
                snap->pixels, snap->texWidth, x, y);

            baseHash = (baseHash ^ base) * 16777619u;
            presentHash = (presentHash ^ present) * 16777619u;
            if (base == present)
                continue;

            diff++;
            quadrant[(y >= height / 2) * 2 + (x >= width / 2)]++;
            if (x < minX) minX = x;
            if (y < minY) minY = y;
            if (x > maxX) maxX = x;
            if (y > maxY) maxY = y;
        }
    }

    sprintf(txtbuffer,
            "VRD PRESENT cycle=%u match=1 baseMap=%u presentMap=%u "
            "diff=%u q=%u/%u/%u/%u bbox=%d,%d-%d,%d "
            "hash=%08X/%08X draws=%u\r\n",
            g_diagCycle, g_diagBaseline.map_id, snap->map_id,
            diff, quadrant[0], quadrant[1], quadrant[2], quadrant[3],
            diff ? minX : -1, diff ? minY : -1,
            diff ? maxX : -1, diff ? maxY : -1,
            baseHash, presentHash,
            g_debugDrawSubmitted - g_diagBaselineDrawCount);
    writeLogFile(txtbuffer);
    g_diagBaselinePending = 0;
}
#endif

static int RectsOverlap(int ax0, int ay0, int ax1, int ay1,
                        int bx0, int by0, int bx1, int by1)
{
    return ax0 < bx1 && ax1 > bx0 && ay0 < by1 && ay1 > by0;
}

static int WrappedReadOverlapsMap(int x, int y, int w, int h,
                                  int mx0, int my0, int mx1, int my1)
{
    int xRemain = w;
    int sx = x & 1023;

    while (xRemain > 0)
    {
        int xLen = xRemain < 1024 - sx ? xRemain : 1024 - sx;
        int yRemain = h;
        int sy = y & 511;

        while (yRemain > 0)
        {
            int yLen = yRemain < 512 - sy ? yRemain : 512 - sy;

            if (RectsOverlap(sx, sy, sx + xLen, sy + yLen,
                             mx0, my0, mx1, my1))
                return 1;

            yRemain -= yLen;
            sy = 0;
        }

        xRemain -= xLen;
        sx = 0;
    }

    return 0;
}

static MappingKind ClassifyReadMapping(int x, int y, int w, int h)
{
    if (g_activeMap.map_valid &&
        WrappedReadOverlapsMap(x, y, w, h,
                               g_activeMap.vram_x0, g_activeMap.vram_y0,
                               g_activeMap.vram_x1, g_activeMap.vram_y1))
        return MAPPING_CURRENT;

    if (!PSXDisplay.InterlacedTest && g_prevMapId != INVALID_MAP_ID &&
        WrappedReadOverlapsMap(x, y, w, h,
                               PreviousPSXDisplay.DisplayPosition.x,
                               PreviousPSXDisplay.DisplayPosition.y,
                               PreviousPSXDisplay.DisplayEnd.x,
                               PreviousPSXDisplay.DisplayEnd.y))
        return MAPPING_PREVIOUS;

    return MAPPING_UNKNOWN;
}

static int SnapshotSourcePriority(const GXFrameSnapshot *snap,
                                  MappingKind mapping)
{
    if (mapping == MAPPING_PREVIOUS)
    {
        if (snap->source == SNAPSHOT_SOURCE_PRESENTED) return 30;
        if (snap->source == SNAPSHOT_SOURCE_UPLOAD_REBUILD) return 20;
        return 10;
    }

    if (snap->source == SNAPSHOT_SOURCE_CURRENT_EFB) return 30;
    if (snap->source == SNAPSHOT_SOURCE_PRESENTED) return 20;
    if (snap->source == SNAPSHOT_SOURCE_UPLOAD_REBUILD) return 10;
    return 0;
}

/* Tile-level T6 selection: one snapshot must cover the whole 16x16 tile with
 * a FULL tile and a sequence strictly newer than the combined psxVuw color
 * epoch.  Tie-break matches the C0 source priority/capture order rules. */
static GXFrameSnapshot *BestOwningSnapshotForTile(int tx, int ty,
                                                  MappingKind mapping,
                                                  uint32_t mapId)
{
    T6SnapshotCandidate cands[2];
    int i, selected;

    memset(cands, 0, sizeof(cands));
    for (i = 0; i < 2; i++)
    {
        GXFrameSnapshot *snap = &g_snapshot[i];

        cands[i].qualified = 0;
        if (snap->valid && snap->map_id == mapId &&
            SnapshotCoversTile(snap, tx, ty) &&
            SnapshotExactRectCoversTile(snap, tx, ty) &&
            SnapshotOwnsColorTileForBarrier(snap, tx, ty))
        {
            cands[i].seq = snap->efbTile[ty & 31][tx & 63].seq;
            cands[i].sourcePriority = SnapshotSourcePriority(snap, mapping);
            cands[i].captureOrder = snap->captureOrder;
            cands[i].qualified = 1;
        }
    }

    selected = T6SelectBestSnapshotTile(cands, 2, NULL);
    return selected > 0 ? &g_snapshot[selected - 1] : NULL;
}

static int SnapshotOwnsPixelForMapping(const GXFrameSnapshot *snap,
                                       int px, int py)
{
    if (snap->exactRectValid &&
        (px < snap->exact_x0 || px >= snap->exact_x1 ||
         py < snap->exact_y0 || py >= snap->exact_y1))
        return 0;

    /* GP0 C0 reads current VRAM.  A CPU write newer than this snapshot wins
     * even when the rectangle belongs to the previous display mapping. */
    return SnapshotOwnsTile(snap, px, py);
}

#ifdef DISP_DEBUG
static void DebugLogC0Selection(int x, int y, int w, int h,
                                MappingKind mapping)
{
    uint32_t targetMap = mapping == MAPPING_CURRENT ?
                         g_activeMap.map_id :
                         (mapping == MAPPING_PREVIOUS ? g_prevMapId : 0);
    int slotIndex;

    for (slotIndex = 0; slotIndex < 2; slotIndex++)
    {
        GXFrameSnapshot *snap = &g_snapshot[slotIndex];
        unsigned int tiles = 0;
        unsigned int owns = 0;
        unsigned int cpuNewer = 0;
        unsigned int notFull = 0;
        unsigned int outside = 0;
        int wrongMap = !snap->valid || snap->map_id != targetMap;
        int yy, xx;

        for (yy = 0; yy < h; yy += VRAM_READBACK_TILE_SIZE)
        {
            int py = (y + min(yy + VRAM_READBACK_TILE_SIZE / 2,
                              h - 1)) & 511;
            for (xx = 0; xx < w; xx += VRAM_READBACK_TILE_SIZE)
            {
                int px = (x + min(xx + VRAM_READBACK_TILE_SIZE / 2,
                                  w - 1)) & 1023;
                const EfbTileState *efb;
                uint64_t cpu;

                tiles++;
                if (wrongMap)
                    continue;
                if (!ContainsPixel(snap, px, py))
                {
                    outside++;
                    continue;
                }

                efb = &snap->efbTile[(py >> 4) & 31][(px >> 4) & 63];
                cpu = g_psxWriteTileEpoch[(py >> 4) & 31][(px >> 4) & 63];
                if (efb->cover != EFB_TILE_FULL || efb->seq == 0)
                    notFull++;
                else if (efb->seq < cpu)
                    cpuNewer++;
                else
                    owns++;
            }
        }

        sprintf(txtbuffer,
                "VRD C0SEL cycle=%u kind=%d target=%u slot=%d "
                "valid=%d map=%u src=%d order=%u wrongMap=%d "
                "tiles=%u own=%u cpuNewer=%u notFull=%u outside=%u "
                "basePending=%d\r\n",
                g_diagCycle, mapping, targetMap, slotIndex,
                snap->valid, snap->map_id, snap->source,
                (unsigned int)snap->captureOrder, wrongMap,
                tiles, owns, cpuNewer, notFull, outside,
                g_diagBaselinePending);
        writeLogFile(txtbuffer);
    }
}
#endif

static GXFrameSnapshot *BestOwningSnapshotForPixel(int px, int py,
                                                    MappingKind mapping,
                                                    uint32_t mapId)
{
    GXFrameSnapshot *best = NULL;
    uint64_t bestSeq = 0;
    int bestPriority = -1;
    int i;

    for (i = 0; i < 2; i++)
    {
        GXFrameSnapshot *snap = &g_snapshot[i];
        const EfbTileState *tile;
        uint64_t seq;
        int priority;

        if (!snap->valid || snap->map_id != mapId ||
            !ContainsPixel(snap, px, py) ||
            !SnapshotOwnsPixelForMapping(snap, px, py))
            continue;

        tile = &snap->efbTile[(py >> 4) & 31][(px >> 4) & 63];
        seq = tile->seq;
        priority = SnapshotSourcePriority(snap, mapping);
        if (best == NULL || seq > bestSeq ||
            (seq == bestSeq && priority > bestPriority) ||
            (seq == bestSeq && priority == bestPriority &&
             snap->captureOrder > best->captureOrder))
        {
            best = snap;
            bestSeq = seq;
            bestPriority = priority;
        }
    }

    return best;
}

static int SnapshotsShareSamplingMap(const GXFrameSnapshot *a,
                                     const GXFrameSnapshot *b)
{
    return a != NULL && b != NULL && a->valid && b->valid &&
           a->map_id == b->map_id &&
           a->vram_x == b->vram_x && a->vram_y == b->vram_y &&
           a->vram_w == b->vram_w && a->vram_h == b->vram_h &&
           a->viewport_x == b->viewport_x &&
           a->viewport_y == b->viewport_y &&
           a->viewport_w == b->viewport_w &&
           a->viewport_h == b->viewport_h;
}

static int SnapshotPixelDiffers(const GXFrameSnapshot *current,
                                const GXFrameSnapshot *baseline,
                                int px, int py)
{
    uint16_t currentRaw, baselineRaw;

    if (!ReadSnapshotPixelRaw(current, px, py, &currentRaw) ||
        !ReadSnapshotPixelRaw(baseline, px, py, &baselineRaw))
        return 0;

    return currentRaw != baselineRaw;
}

static void InvalidateTileRunRect(void *user, int x0, int y0, int x1, int y1)
{
    (void)user;
    InvalidateTextureArea(x0, y0, x1 - x0, y1 - y0);
}

static void MergeReadbackToPsxVuwFiltered(
    int x, int y, int w, int h,
    const GXFrameSnapshot *requiredSnapshot,
    const GXFrameSnapshot *baseline)
{
    unsigned char writtenTile[VRAM_READBACK_TILE_Y][VRAM_READBACK_TILE_X];
    unsigned int tilePixels[VRAM_READBACK_TILE_Y][VRAM_READBACK_TILE_X];
    uint64_t tileSnapSeq[VRAM_READBACK_TILE_Y][VRAM_READBACK_TILE_X];
    unsigned char tileMixed[VRAM_READBACK_TILE_Y][VRAM_READBACK_TILE_X];
    MappingKind mapping;
    uint32_t mapId;
    int yy, xx, ty, tx;
#ifdef DISP_DEBUG
    unsigned int oldNonZero[2] = {0, 0};
    unsigned int snapNonZero[2] = {0, 0};
    unsigned int newNonZero[2] = {0, 0};
    unsigned int snapPixels[2] = {0, 0};
    unsigned int oldHash[2] = {2166136261u, 2166136261u};
    unsigned int snapHash[2] = {2166136261u, 2166136261u};
    unsigned int newHash[2] = {2166136261u, 2166136261u};
    unsigned int paletteChecksBefore;
    unsigned int totalInvalidatedBefore;
    uint64_t invalTicksStart;
    uint64_t invalTicksEnd;
#endif

#ifdef DISP_DEBUG
    g_lastMergedPixels = 0;
    g_lastMergedChangedPixels = 0;
    g_lastMergedMaskOnlyPixels = 0;
    g_lastMergedRgbChangedPixels = 0;
    g_lastMergeOldMaskPixels = 0;
    g_lastMergeNewMaskPixels = 0;
    g_lastMergeOldHash = 2166136261u;
    g_lastMergeNewHash = 2166136261u;
    g_lastMergedCurrentPixels = 0;
    g_lastMergedPresentedPixels = 0;
    g_lastMergedRebuildPixels = 0;
    g_lastReadbackInvalidateRuns = 0;
    g_lastReadbackPaletteEntryChecks = 0;
    g_lastReadbackTotalInvalidated = 0;
    g_lastReadbackInvalidateUs = 0;
#endif

    if (!ReadbackEnabled())
        return;

    mapping = ClassifyReadMapping(x, y, w, h);
    g_lastReadMapping = mapping;
    if (mapping == MAPPING_CURRENT)
        mapId = g_activeMap.map_id;
    else if (mapping == MAPPING_PREVIOUS)
        mapId = g_prevMapId;
    else
        return;

    memset(writtenTile, 0, sizeof(writtenTile));
    memset(tilePixels, 0, sizeof(tilePixels));
    memset(tileSnapSeq, 0, sizeof(tileSnapSeq));
    memset(tileMixed, 0, sizeof(tileMixed));

    for (yy = 0; yy < h; yy++)
    {
        int py = (y + yy) & 511;
        for (xx = 0; xx < w; xx++)
        {
            int px = (x + xx) & 1023;
            uint16_t old = GETLE16(psxVuw + py * 1024 + px);
            GXFrameSnapshot *snap = BestOwningSnapshotForPixel(px, py,
                                                               mapping,
                                                               mapId);
#ifdef DISP_DEBUG
            int half = (yy < (h / 2)) ? 0 : 1;

            if (old & 0x7fff)
                oldNonZero[half]++;
            oldHash[half] = (oldHash[half] ^ old) * 16777619u;
#endif

            if (snap == NULL)
            {
#ifdef DISP_DEBUG
                if (old & 0x7fff)
                    newNonZero[half]++;
                newHash[half] = (newHash[half] ^ old) * 16777619u;
#endif
                continue;
            }

            /* A FULL tile means every EFB pixel is readable, not that every
             * pixel was changed by the later partial polygon.  DC2 needs only
             * the post-rebuild delta; retaining untouched psxVuw pixels also
             * avoids visible 16x16 resampling boxes on real hardware. */
            if (requiredSnapshot != NULL &&
                (snap != requiredSnapshot || baseline == NULL ||
                 !SnapshotPixelDiffers(snap, baseline, px, py)))
            {
#ifdef DISP_DEBUG
                if (old & 0x7fff)
                    newNonZero[half]++;
                newHash[half] = (newHash[half] ^ old) * 16777619u;
#endif
                continue;
            }

            uint16_t color = ReadSnapshotPixelToPSX(snap, px, py);
            uint16_t psx = (color & 0x7FFF) | (old & 0x8000);
            {
                int ttx = (px >> 4) & 63;
                int tty = (py >> 4) & 31;
                uint64_t snapTileSeq = snap->efbTile[tty][ttx].seq;

                if (!T6C0ColorCommitAllowed(
                        snapTileSeq,
                        g_psxMaterializedColorTileEpoch[tty][ttx]))
                    continue;
                tilePixels[tty][ttx]++;
                if (tilePixels[tty][ttx] == 1)
                    tileSnapSeq[tty][ttx] = snapTileSeq;
                else if (tileSnapSeq[tty][ttx] != snapTileSeq)
                    tileMixed[tty][ttx] = 1;
            }

#ifdef DISP_DEBUG
            snapPixels[half]++;
            if (color & 0x7fff)
                snapNonZero[half]++;
            if (psx & 0x7fff)
                newNonZero[half]++;
            snapHash[half] = (snapHash[half] ^ color) * 16777619u;
            newHash[half] = (newHash[half] ^ psx) * 16777619u;
#endif

#ifdef DISP_DEBUG
            g_lastMergeOldHash =
                (g_lastMergeOldHash ^ old) * 16777619u;
            g_lastMergeNewHash =
                (g_lastMergeNewHash ^ psx) * 16777619u;
            if (old & 0x8000)
                g_lastMergeOldMaskPixels++;
            if (psx & 0x8000)
                g_lastMergeNewMaskPixels++;
            if (old != psx)
            {
                g_lastMergedChangedPixels++;
                if ((old & 0x7FFF) == (psx & 0x7FFF))
                    g_lastMergedMaskOnlyPixels++;
                else
                    g_lastMergedRgbChangedPixels++;
            }
            if (snap->source == SNAPSHOT_SOURCE_CURRENT_EFB)
                g_lastMergedCurrentPixels++;
            else if (snap->source == SNAPSHOT_SOURCE_PRESENTED)
                g_lastMergedPresentedPixels++;
            else if (snap->source == SNAPSHOT_SOURCE_UPLOAD_REBUILD)
                g_lastMergedRebuildPixels++;
            g_lastMergedPixels++;
#endif
            PUTLE16(psxVuw + py * 1024 + px, psx);
            writtenTile[(py >> 4) & 31][(px >> 4) & 63] = TRUE;
        }
    }

#ifdef DISP_DEBUG
    sprintf(txtbuffer,
            "VRH MERGE rect=%d,%d %dx%d split=%d "
            "old=%u/%08X,%u/%08X "
            "snap=%u:%u/%08X,%u:%u/%08X "
            "new=%u/%08X,%u/%08X\r\n",
            x, y, w, h, y + h / 2,
            oldNonZero[0], oldHash[0], oldNonZero[1], oldHash[1],
            snapPixels[0], snapNonZero[0], snapHash[0],
            snapPixels[1], snapNonZero[1], snapHash[1],
            newNonZero[0], newHash[0], newNonZero[1], newHash[1]);
    writeLogFile(txtbuffer);
#endif

    /* A C0 merge that fully coordinated a whole tile from one snapshot may
     * promote the materialized color epoch; partial reads never claim a tile.
     * This runs after pixel writeback and before cache invalidation. */
    {
        int ty, tx;

        for (ty = 0; ty < VRAM_READBACK_TILE_Y; ty++)
            for (tx = 0; tx < VRAM_READBACK_TILE_X; tx++)
                g_psxMaterializedColorTileEpoch[ty][tx] =
                    T6C0FullTilePromotionEpoch(
                        (int)tilePixels[ty][tx], !tileMixed[ty][tx],
                        tileSnapSeq[ty][tx],
                        g_psxMaterializedColorTileEpoch[ty][tx]);
    }

    /* T5 first pass: coalesce horizontal written-tile runs so a full-width
     * readback emits one invalidation per row instead of one per 16x16 tile.
     * Pixel merge, mask selection, and snapshot ownership are unchanged. */
#ifdef DISP_DEBUG
    paletteChecksBefore = g_texturePaletteEntryChecks;
    totalInvalidatedBefore = g_textureTotalInvalidatedEntries;
    invalTicksStart = (uint64_t)gettime();
#endif
    {
        int runs = ForEachHorizontalTileRun(&writtenTile[0][0],
                                            VRAM_READBACK_TILE_X,
                                            VRAM_READBACK_TILE_Y, 16,
                                            InvalidateTileRunRect, NULL);
#ifdef DISP_DEBUG
        invalTicksEnd = (uint64_t)gettime();
        g_lastReadbackInvalidateRuns = (unsigned int)runs;
        g_lastReadbackPaletteEntryChecks =
            g_texturePaletteEntryChecks - paletteChecksBefore;
        g_lastReadbackTotalInvalidated =
            g_textureTotalInvalidatedEntries - totalInvalidatedBefore;
        g_lastReadbackInvalidateUs =
            (unsigned int)ticks_to_microsecs(invalTicksEnd - invalTicksStart);
#else
        (void)runs;
#endif
    }

#ifdef DISP_DEBUG
    sprintf(txtbuffer,
            "VRH INVAL runs=%u palChecks=%u totalInvalidated=%u us=%u "
            "stdUp=%u wndUp=%u\r\n",
            g_lastReadbackInvalidateRuns,
            g_lastReadbackPaletteEntryChecks,
            g_lastReadbackTotalInvalidated,
            g_lastReadbackInvalidateUs,
            g_textureStandardUploads,
            g_textureWindowUploads);
    writeLogFile(txtbuffer);
#endif
}

static void MergeReadbackToPsxVuw(int x, int y, int w, int h)
{
    MergeReadbackToPsxVuwFiltered(x, y, w, h, NULL, NULL);
}

static int RectIntersectsTile(int x0, int y0, int x1, int y1,
                              int tx, int ty)
{
    int px0 = tx << 4;
    int py0 = ty << 4;

    return x0 < px0 + VRAM_READBACK_TILE_SIZE &&
           px0 < x1 &&
           y0 < py0 + VRAM_READBACK_TILE_SIZE &&
           py0 < y1;
}

static MappingKind ClassifyTileMapping(int tx, int ty)
{
    int kind = T6ClassifyTileMap(
        tx, ty,
        g_activeMap.map_valid ? g_activeMap.vram_x0 : 0,
        g_activeMap.map_valid ? g_activeMap.vram_y0 : 0,
        g_activeMap.map_valid ? g_activeMap.vram_x1 : 0,
        g_activeMap.map_valid ? g_activeMap.vram_y1 : 0,
        PreviousPSXDisplay.DisplayPosition.x,
        PreviousPSXDisplay.DisplayPosition.y,
        PreviousPSXDisplay.DisplayEnd.x,
        PreviousPSXDisplay.DisplayEnd.y,
        !PSXDisplay.InterlacedTest && g_prevMapId != INVALID_MAP_ID);

    return kind == T6_MAP_CURRENT ? MAPPING_CURRENT :
           (kind == T6_MAP_PREVIOUS ? MAPPING_PREVIOUS :
                                      MAPPING_UNKNOWN);
}

typedef struct {
    uint64_t seq;
    EfbTileCover cover;
    int present;
    uint32_t mapId;
    const GXDisplayMap *map;
    EfbTileState (*tiles)[VRAM_READBACK_TILE_X];
} T6PhysicalTileState;

static int TilePhysicalEfbState(int tx, int ty, MappingKind mapping,
                                uint32_t targetMapId,
                                T6PhysicalTileState *out)
{
    const GXDisplayMap *map = NULL;
    EfbTileState (*tiles)[VRAM_READBACK_TILE_X] = NULL;
    const EfbTileState *st;

    memset(out, 0, sizeof(*out));

    if (mapping == MAPPING_CURRENT)
    {
        map = &g_activeMap;
        tiles = g_efbTileState;
        if (!map->map_valid || map->map_id != targetMapId)
            return 0;
    }
    else if (mapping == MAPPING_PREVIOUS)
    {
        if (g_pendingPresentedReady &&
            (g_pendingPresentedMap.vram_x0 != g_activeMap.vram_x0 ||
             g_pendingPresentedMap.vram_y0 != g_activeMap.vram_y0 ||
             g_pendingPresentedMap.vram_x1 != g_activeMap.vram_x1 ||
             g_pendingPresentedMap.vram_y1 != g_activeMap.vram_y1) &&
            RectIntersectsTile(g_pendingPresentedMap.vram_x0,
                               g_pendingPresentedMap.vram_y0,
                               g_pendingPresentedMap.vram_x1,
                               g_pendingPresentedMap.vram_y1,
                               tx, ty))
        {
            map = &g_pendingPresentedMap;
            tiles = g_pendingPresentedTile;
        }
        else
            return 0;
    }
    else
        return 0;

    if (map == NULL || !map->map_valid || !map->content_valid)
        return 0;

    st = &tiles[ty & 31][tx & 63];
    out->seq = st->seq;
    out->cover = st->cover;
    out->present = 1;
    out->mapId = map->map_id;
    out->map = map;
    out->tiles = tiles;
    return 1;
}

/* Physical EFB ownership: current must carry the active map id; previous is
 * only accepted through the pending-presented buffer (INVALID_MAP_ID) that
 * will be captured under the target previous map id. */
static int TilePhysicalOwnsTargetMap(const T6PhysicalTileState *phys,
                                     MappingKind mapping,
                                     uint32_t targetMapId)
{
    int kind;

    if (phys == NULL || !phys->present)
        return 0;

    kind = mapping == MAPPING_CURRENT ? T6_MAP_CURRENT :
           (mapping == MAPPING_PREVIOUS ? T6_MAP_PREVIOUS :
                                          T6_MAP_UNKNOWN);
    return T6PhysicalOwnsTargetMap((int)phys->mapId, kind,
                                   targetMapId, g_prevMapId);
}

static int RebuildBaselineMatchesSnapshot(const GXFrameSnapshot *snap)
{
    return g_rebuildBaseline.valid && snap != NULL &&
           g_rebuildBaseline.map_id == snap->map_id &&
           SnapshotsShareSamplingMap(snap, &g_rebuildBaseline);
}

static int RebuildBaselineUsableForTile(const GXFrameSnapshot *snap,
                                        int tx, int ty)
{
    const EfbTileState *base;

    if (!RebuildBaselineMatchesSnapshot(snap))
        return 0;

    base = &g_rebuildBaseline.efbTile[ty & 31][tx & 63];
    return base->cover == EFB_TILE_FULL && base->seq != 0 &&
           base->seq >= g_psxWriteTileEpoch[ty & 31][tx & 63];
}

static int CaptureEfbForMaterialize(GXFrameSnapshot *slot,
                                    MappingKind mapping,
                                    uint32_t targetMapId,
                                    int tx, int ty)
{
    GXDisplayMap map;
    EfbTileState (*tiles)[VRAM_READBACK_TILE_X];

    if (mapping == MAPPING_CURRENT)
    {
        if (!CanCaptureActiveEfb())
            return 0;
        map = g_activeMap;
        tiles = g_efbTileState;
    }
    else if (mapping == MAPPING_PREVIOUS)
    {
        if (!g_pendingPresentedReady ||
            targetMapId != g_prevMapId ||
            !RectIntersectsTile(g_pendingPresentedMap.vram_x0,
                                g_pendingPresentedMap.vram_y0,
                                g_pendingPresentedMap.vram_x1,
                                g_pendingPresentedMap.vram_y1,
                                tx, ty))
            return 0;
        map = g_pendingPresentedMap;
        tiles = g_pendingPresentedTile;

        if ((STATUSREG & GPUSTATUS_RGB24) || PSXDisplay.RGB24 ||
            g_efbContaminated || g_mixedMappingSeen || g_untrackedEfbWrite)
            return 0;

        map.map_id = targetMapId;
        map.map_valid = 1;
        map.content_valid = 1;
        map.content_dirty = 1;
    }
    else
        return 0;

    return CaptureEfbSnapshotWithTiles(
        slot, &map, tiles,
        mapping == MAPPING_CURRENT ? SNAPSHOT_SOURCE_CURRENT_EFB :
                                     SNAPSHOT_SOURCE_PRESENTED);
}

typedef struct {
    VramMaterializeSession *session;
    unsigned char seen[T6_VRAM_TILE_Y][T6_VRAM_TILE_X];
} T6MaterializeObserve;

static void ObserveMaterializeTile(void *user, int tx, int ty)
{
    T6MaterializeObserve *obs = (T6MaterializeObserve *)user;
    T6PhysicalTileState phys;
    GXFrameSnapshot *snap;
    MappingKind mapping;
    uint32_t mapId;
    uint64_t psxColor, snapSeq, efbSeq;
    int slotIndex = -1;

    if (obs->seen[ty & 31][tx & 63])
        return;
    obs->seen[ty & 31][tx & 63] = 1;

    mapping = ClassifyTileMapping(tx, ty);
    if (mapping == MAPPING_CURRENT)
        mapId = g_activeMap.map_id;
    else if (mapping == MAPPING_PREVIOUS)
        mapId = g_prevMapId;
    else
        mapId = 0;

    memset(&phys, 0, sizeof(phys));
    if (mapping != MAPPING_UNKNOWN)
    {
        TilePhysicalEfbState(tx, ty, mapping, mapId, &phys);
        if (!TilePhysicalOwnsTargetMap(&phys, mapping, mapId))
            memset(&phys, 0, sizeof(phys));
    }

    snap = BestOwningSnapshotForTile(tx, ty, mapping, mapId);

    psxColor = PsxVuwColorTileEpoch(
        g_psxWriteTileEpoch[ty & 31][tx & 63],
        g_psxMaterializedColorTileEpoch[ty & 31][tx & 63]);
    snapSeq = snap ? snap->efbTile[ty & 31][tx & 63].seq : 0;
    efbSeq = phys.present && phys.seq > snapSeq ? phys.seq : snapSeq;

    if (snap != NULL && snapSeq >= efbSeq)
        slotIndex = snap == &g_snapshot[0] ? 0 : 1;

    T6MaterializeSessionObserveTile(obs->session, tx, ty,
                                    psxColor, efbSeq, snapSeq,
                                    mapId, slotIndex, (int)mapping,
                                    phys.present, phys.seq);
}

static int ValidateMaterializeTile(void *user, int tx, int ty,
                                   uint64_t requiredSeq,
                                   VramTileFreshnessInput *in)
{
    VramMaterializeSession *session = (VramMaterializeSession *)user;
    T6PhysicalTileState phys;
    GXFrameSnapshot *snap;
    MappingKind mapping;
    uint32_t mapId;
    const EfbTileState *efb;

#if T6_BARRIER_DIAG
    g_vramReadBarrierLastReason = T6_REASON_NONE;
#endif
    memset(&phys, 0, sizeof(phys));
    mapping = ClassifyTileMapping(tx, ty);
    mapId = T6MaterializeSessionRequiredMapId(session, tx, ty);

    if (mapping == MAPPING_UNKNOWN || mapId == 0)
    {
        in->untrackedEfb = 1;
#if T6_BARRIER_DIAG
        g_vramReadBarrierLastReason = T6_REASON_UNKNOWN_MAP;
#endif
        return 1;
    }

    TilePhysicalEfbState(tx, ty, mapping, mapId, &phys);
    if (!TilePhysicalOwnsTargetMap(&phys, mapping, mapId))
        memset(&phys, 0, sizeof(phys));

    snap = BestOwningSnapshotForTile(tx, ty, mapping, mapId);

    in->cpuWriteEpoch = g_psxWriteTileEpoch[ty & 31][tx & 63];
    in->materializedColorEpoch =
        g_psxMaterializedColorTileEpoch[ty & 31][tx & 63];
    in->snapshotSeq = snap ? snap->efbTile[ty & 31][tx & 63].seq : 0;
    in->rgb24 = (STATUSREG & GPUSTATUS_RGB24) || PSXDisplay.RGB24 ||
                (phys.map != NULL && phys.map->rgb24);
    in->contaminated = g_efbContaminated;
    in->mixedMapping = g_mixedMappingSeen;
    in->untrackedEfb = g_untrackedEfbWrite;
    in->hasValidSnapshot =
        snap != NULL && in->snapshotSeq >= requiredSeq;
    in->baselinePresentForSnapshot =
        snap != NULL && RebuildBaselineMatchesSnapshot(snap);
    in->baselineUsable =
        snap != NULL && RebuildBaselineUsableForTile(snap, tx, ty);

    efb = snap ? &snap->efbTile[ty & 31][tx & 63] : NULL;
    in->efbCoverFull =
        efb != NULL && efb->cover == EFB_TILE_FULL &&
        SnapshotExactRectCoversTile(snap, tx, ty);
#if T6_BARRIER_DIAG
    if (in->rgb24)
        g_vramReadBarrierLastReason = T6_REASON_RGB24;
    else if (in->contaminated || in->mixedMapping || in->untrackedEfb)
        g_vramReadBarrierLastReason = T6_REASON_HAZARD;
    else if (!in->efbCoverFull)
        g_vramReadBarrierLastReason = T6_REASON_PARTIAL;
    else if (!in->hasValidSnapshot)
        g_vramReadBarrierLastReason = T6_REASON_CAPTURE_FAIL;
    else if (in->baselinePresentForSnapshot && !in->baselineUsable)
        g_vramReadBarrierLastReason = T6_REASON_BASELINE_STALE;
#endif
    return 1;
}

static void WriteMaterializeTile(void *user, VramMaterializePlan *plan,
                                 int tx, int ty, uint64_t requiredSeq)
{
    VramMaterializeSession *session = (VramMaterializeSession *)user;
    MappingKind mapping;
    uint32_t mapId;
    GXFrameSnapshot *snap;
    int baselineDelta;
    int px, py;

    (void)requiredSeq;
    mapping = ClassifyTileMapping(tx, ty);
    mapId = T6MaterializeSessionRequiredMapId(session, tx, ty);
    if (mapping == MAPPING_UNKNOWN || mapId == 0)
        return;

    snap = BestOwningSnapshotForTile(tx, ty, mapping, mapId);
    if (snap == NULL)
        return;
    baselineDelta = RebuildBaselineUsableForTile(snap, tx, ty);

    for (py = ty << 4; py < (ty << 4) + VRAM_READBACK_TILE_SIZE; py++)
        for (px = tx << 4; px < (tx << 4) + VRAM_READBACK_TILE_SIZE; px++)
        {
            unsigned short old = GETLE16(psxVuw + (py << 10) + px);
            unsigned short color;
            unsigned short psx;

            if (baselineDelta &&
                !SnapshotPixelDiffers(snap, &g_rebuildBaseline, px, py))
                continue;

            color = ReadSnapshotPixelToPSX(snap, px, py);
            psx = T6MergePixelColor(old, color);
            if (psx == old)
                continue;

            PUTLE16(psxVuw + (py << 10) + px, psx);
            T6MaterializePlanMarkChanged(plan, tx, ty);
        }
}

static uint64_t MaterializeSnapshotSeq(void *user, int tx, int ty)
{
    VramMaterializeSession *session = (VramMaterializeSession *)user;
    MappingKind mapping;
    uint32_t mapId;
    GXFrameSnapshot *snap;

    mapping = ClassifyTileMapping(tx, ty);
    mapId = T6MaterializeSessionRequiredMapId(session, tx, ty);
    if (mapping == MAPPING_UNKNOWN || mapId == 0)
        return T6MaterializeSessionRequiredSeq(session, tx, ty);

    snap = BestOwningSnapshotForTile(tx, ty, mapping, mapId);
    if (snap != NULL)
        return snap->efbTile[ty & 31][tx & 63].seq;
    return T6MaterializeSessionRequiredSeq(session, tx, ty);
}

static void ApplyMaterializeEpoch(void *user, int tx, int ty, uint64_t seq)
{
    (void)user;
    if (seq > g_psxMaterializedColorTileEpoch[ty & 31][tx & 63])
        g_psxMaterializedColorTileEpoch[ty & 31][tx & 63] = seq;
}

/* Two-phase materialize: prove every hazard tile first, then write, commit
 * epochs, and invalidate only after the whole batch is safe. */
static VramFreshResult MaterializeVramReadCore(const VramReadDependency *dep)
{
    VramMaterializeSession session;
    T6MaterializeObserve obs;
    int slotIndex;
    int oldestIndex;

    if (!ReadbackEnabled() || dep == NULL || dep->count == 0 ||
        dep->count > T6_DEP_RECT_CAPACITY)
        return VRAM_FRESH_UNRESOLVED;

#if T6_BARRIER_DIAG
    g_vramReadBarrierLastReason = T6_REASON_NONE;
    g_vramReadBarrierCopyTex = 0;
    g_vramReadBarrierChangedTiles = 0;
    g_vramReadBarrierInvalidateRuns = 0;
#endif

    T6MaterializeSessionInit(&session, &g_t6Workspace);
    memset(&obs, 0, sizeof(obs));
    obs.session = &session;
    ForEachVramReadDependencyTile(dep, 1024, 512,
                                  ObserveMaterializeTile, &obs);

    if (session.overflow)
    {
#if T6_BARRIER_DIAG
        g_vramReadBarrierLastReason = T6_REASON_OVERFLOW;
#endif
        return VRAM_FRESH_UNRESOLVED;
    }

    if (session.captureRequired)
    {
        GXFrameSnapshot *oldest = OldestSnapshotSlot();

        oldestIndex = oldest == &g_snapshot[0] ? 0 : 1;
        if (session.captureConflict ||
            !T6MaterializeSessionSelectCaptureSlot(
                &session, oldestIndex, &slotIndex))
        {
#if T6_BARRIER_DIAG
            {
                T6CaptureDiagInput diag;

                T6CaptureDiagFill(
                    &diag,
                    (STATUSREG & GPUSTATUS_RGB24) || PSXDisplay.RGB24,
                    g_activeMap.rgb24,
                    g_pendingPresentedReady ? g_pendingPresentedMap.rgb24 : 0,
                    (int)session.captureMappingKind == MAPPING_PREVIOUS ?
                        T6_MAP_PREVIOUS : T6_MAP_CURRENT,
                    g_efbContaminated, g_mixedMappingSeen,
                    g_untrackedEfbWrite,
                    session.captureMapId == 0);
                g_vramReadBarrierLastReason =
                    T6ClassifyCaptureFailure(&diag);
            }
#endif
            return VRAM_FRESH_UNRESOLVED;
        }

#if T6_BARRIER_DIAG
        {
            T6CaptureDiagInput diag;

            T6CaptureDiagFill(
                &diag,
                (STATUSREG & GPUSTATUS_RGB24) || PSXDisplay.RGB24,
                g_activeMap.rgb24,
                g_pendingPresentedReady ? g_pendingPresentedMap.rgb24 : 0,
                (int)session.captureMappingKind == MAPPING_PREVIOUS ?
                    T6_MAP_PREVIOUS : T6_MAP_CURRENT,
                g_efbContaminated, g_mixedMappingSeen,
                g_untrackedEfbWrite,
                session.captureMapId == 0);
            g_vramReadBarrierLastReason =
                T6ClassifyCaptureFailure(&diag);
        }
#endif
        if (!CaptureEfbForMaterialize(
                &g_snapshot[slotIndex],
                (MappingKind)session.captureMappingKind,
                session.captureMapId,
                session.captureTileX, session.captureTileY))
        {
            return VRAM_FRESH_UNRESOLVED;
        }
#if T6_BARRIER_DIAG
        g_vramReadBarrierLastReason = T6_REASON_NONE;
        T6CopyTexCapture(&g_vramReadBarrierCopyTex,
                         &g_vramReadBarrierCopyTexTotal);
#endif
    }

    if (session.plan.hazardCount == 0)
        return VRAM_FRESH_NO_ACTION;

    /* Re-prove the original hazard batch; capture may have replaced a slot,
     * so a missing snapshot now means the whole barrier is unresolved. */
    if (!T6MaterializeSessionValidate(&session,
                                      ValidateMaterializeTile, &session))
        return VRAM_FRESH_UNRESOLVED;

    T6MaterializeSessionWrite(&session, WriteMaterializeTile, &session);
#if T6_BARRIER_DIAG
    g_vramReadBarrierChangedTiles =
        (unsigned int)session.plan.changedCount;
#endif
    T6MaterializeSessionCommitEpoch(&session,
                                    MaterializeSnapshotSeq,
                                    ApplyMaterializeEpoch, NULL);

    if (session.plan.changedCount > 0)
    {
        int runs = T6MaterializeSessionInvalidateChanged(
            &session, InvalidateTileRunRect, NULL);
#if T6_BARRIER_DIAG
        g_vramReadBarrierInvalidateRuns = (unsigned int)runs;
#endif
        (void)runs;
    }

    return VRAM_FRESH_MATERIALIZED;
}

static VramFreshResult MaterializeVramRead(const VramReadDependency *dep)
{
    VramFreshResult result;

    if (!ReadbackEnabled() || dep == NULL || dep->count == 0 ||
        dep->count > T6_DEP_RECT_CAPACITY)
        return VRAM_FRESH_UNRESOLVED;

    if (!T6WorkspaceBegin(&g_t6WorkspaceDepth))
        return VRAM_FRESH_UNRESOLVED;

    result = MaterializeVramReadCore(dep);
    T6WorkspaceEnd(&g_t6WorkspaceDepth);
    return result;
}

/* Texture-path barrier entry.  Returns UNRESOLVED on reentrancy/disabled
 * state so a nested lookup can never paper over stale VRAM with success. */
static VramFreshResult EnsureVramReadFresh(
    const VramReadDependency *dependency)
{
    VramFreshResult result;
#if T6_BARRIER_DIAG
    uint64_t ticksStart = (uint64_t)gettime();
    uint64_t ticksEnd;
#endif

    if (!ReadbackEnabled() || dependency == NULL ||
        dependency->count == 0 || dependency->count > T6_DEP_RECT_CAPACITY)
        return VRAM_FRESH_UNRESOLVED;

#if T6_BARRIER_DIAG
    g_vramReadBarrierCalls++;
#endif

    if (!T6WorkspaceBegin(&g_vramReadBarrierDepth))
    {
#if T6_BARRIER_DIAG
        g_vramReadBarrierReentrant++;
        g_vramReadBarrierUnresolved++;
        g_vramReadBarrierLastReason = T6_REASON_REENTRANT;
        if (g_vramReadBarrierReasonLogs[T6_REASON_REENTRANT] < 8)
        {
            g_vramReadBarrierReasonLogs[T6_REASON_REENTRANT]++;
            sprintf(txtbuffer,
                    "TRB REENTRANT depth=%d\r\n",
                    g_vramReadBarrierDepth);
            writeLogFile(txtbuffer);
        }
#endif
        return VRAM_FRESH_UNRESOLVED;
    }

    result = MaterializeVramRead(dependency);
    T6WorkspaceEnd(&g_vramReadBarrierDepth);

#if T6_BARRIER_DIAG
    ticksEnd = (uint64_t)gettime();
    if (result == VRAM_FRESH_NO_ACTION)
        g_vramReadBarrierNoAction++;
    else if (result == VRAM_FRESH_MATERIALIZED)
        g_vramReadBarrierMaterialized++;
    else
        g_vramReadBarrierUnresolved++;

    if (result == VRAM_FRESH_UNRESOLVED)
    {
        int reason = g_vramReadBarrierLastReason;

        if (reason <= 0 || reason >= T6_REASON_COUNT)
            reason = T6_REASON_NONE;
        if (g_vramReadBarrierReasonLogs[reason] < 8)
        {
            g_vramReadBarrierReasonLogs[reason]++;
            sprintf(txtbuffer,
                    "TRB WND result=unresolved reason=%d calls=%u "
                    "copyTex=%u copyTexTotal=%u changed=%u invalRuns=%u "
                    "us=%u\r\n",
                    reason, g_vramReadBarrierCalls,
                    g_vramReadBarrierCopyTex,
                    g_vramReadBarrierCopyTexTotal,
                    g_vramReadBarrierChangedTiles,
                    g_vramReadBarrierInvalidateRuns,
                    (unsigned int)ticks_to_microsecs(
                        ticksEnd - ticksStart));
            writeLogFile(txtbuffer);
        }
    }
    else if (g_vramReadBarrierSuccessLogs < 32)
    {
        g_vramReadBarrierSuccessLogs++;
        sprintf(txtbuffer,
                "TRB WND result=%d calls=%u noAction=%u materialized=%u "
                "copyTex=%u copyTexTotal=%u changed=%u invalRuns=%u "
                "us=%u\r\n",
                (int)result, g_vramReadBarrierCalls,
                g_vramReadBarrierNoAction,
                g_vramReadBarrierMaterialized,
                g_vramReadBarrierCopyTex,
                g_vramReadBarrierCopyTexTotal,
                g_vramReadBarrierChangedTiles,
                g_vramReadBarrierInvalidateRuns,
                (unsigned int)ticks_to_microsecs(
                    ticksEnd - ticksStart));
        writeLogFile(txtbuffer);
    }
#endif

    return result;
}

/* Dedicated T6-B synthetic entry.  No production texture or MoveImage caller
 * uses this yet; it exists so later stages and tests can drive the core. */
void T6_MaterializeVramReadForTest(int x, int y, int w, int h)
{
    VramReadDependency dep;

    if (!ReadbackEnabled())
        return;

    memset(&dep, 0, sizeof(dep));
    if (!BuildMoveSourceDependency(x, y, w, h, 1024, 512, &dep))
        return;

    MaterializeVramRead(&dep);
}

/* GP0 80h reads VRAM internally; it does not pass through the C0 transfer
 * path.  While a double-buffered game builds the next page, GP1 still names
 * the old display page, so the physical EFB is described by the pending
 * presented map rather than g_activeMap.  Materialize that EFB before the
 * CPU-side MoveImage copy consumes psxVuw. */
static void MaterializeEfbForVramMove(int x, int y, int w, int h)
{
    MappingKind mapping;
    uint32_t targetMapId;
    GXFrameSnapshot *capturedSnapshot = NULL;
    const GXFrameSnapshot *baseline = NULL;
    int captured = 0;

    if (!ReadbackEnabled() || w <= 0 || h <= 0)
        return;

    mapping = ClassifyReadMapping(x, y, w, h);
    if (mapping == MAPPING_CURRENT)
        targetMapId = g_activeMap.map_id;
    else if (mapping == MAPPING_PREVIOUS)
        targetMapId = g_prevMapId;
    else
        return;

    if (g_pendingPresentedReady &&
        (g_pendingPresentedMap.vram_x0 != g_activeMap.vram_x0 ||
         g_pendingPresentedMap.vram_y0 != g_activeMap.vram_y0 ||
         g_pendingPresentedMap.vram_x1 != g_activeMap.vram_x1 ||
         g_pendingPresentedMap.vram_y1 != g_activeMap.vram_y1) &&
        !(STATUSREG & GPUSTATUS_RGB24) && !PSXDisplay.RGB24 &&
        !g_efbContaminated && !g_mixedMappingSeen &&
        !g_untrackedEfbWrite &&
        WrappedReadOverlapsMap(x, y, w, h,
                               g_pendingPresentedMap.vram_x0,
                               g_pendingPresentedMap.vram_y0,
                               g_pendingPresentedMap.vram_x1,
                               g_pendingPresentedMap.vram_y1))
    {
        GXDisplayMap map = g_pendingPresentedMap;
        GXFrameSnapshot *slot = OldestSnapshotSlot();

        map.map_id = targetMapId;
        map.map_valid = 1;
        map.content_valid = 1;
        map.content_dirty = 1;

        captured = CaptureEfbSnapshotWithTiles(
            slot, &map, g_pendingPresentedTile,
            SNAPSHOT_SOURCE_CURRENT_EFB);
        if (captured)
            capturedSnapshot = slot;
        g_lastCaptureResult = captured ? 2 : -7;
    }
    else if (mapping == MAPPING_CURRENT)
    {
        TryCaptureLiveFrame();
        captured = g_lastCaptureResult == 1;
    }
    else
    {
        g_lastCaptureResult = -8;
    }

    if (capturedSnapshot != NULL &&
        SnapshotsShareSamplingMap(capturedSnapshot, &g_rebuildBaseline))
        baseline = &g_rebuildBaseline;

    if (baseline != NULL)
        MergeReadbackToPsxVuwFiltered(x, y, w, h,
                                     capturedSnapshot, baseline);
    else
        MergeReadbackToPsxVuw(x, y, w, h);

#ifdef DISP_DEBUG
    sprintf(txtbuffer,
            "VRB MOVE_READ kind=%d capture=%d captured=%d rect=%d,%d %dx%d "
            "target=%u pending=%d/%u delta=%d merged=%u changed=%u "
            "src=%u/%u/%u\r\n",
            mapping, g_lastCaptureResult, captured, x, y, w, h,
            targetMapId, g_pendingPresentedReady,
            g_pendingPresentedMap.map_id, baseline != NULL,
            g_lastMergedPixels,
            g_lastMergedChangedPixels, g_lastMergedCurrentPixels,
            g_lastMergedPresentedPixels, g_lastMergedRebuildPixels);
    writeLogFile(txtbuffer);
#endif
}

#endif /* GPU_VRAM_READBACK_INC */
