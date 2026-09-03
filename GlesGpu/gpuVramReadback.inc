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

typedef enum {
    CAPTURE_REJECTED = 0,
    CAPTURE_COMPLETE = 1,
    CAPTURE_QUEUED = 2
} CaptureResult;

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

/* A GX copy is first written here.  Only the draw-sync callback may mark it
 * ready, and only the emulation thread may publish it into g_snapshot[]. */
static GXFrameSnapshot g_asyncSnapshot;
static GXFrameSnapshot *g_asyncTarget;
static volatile BOOL g_asyncCaptureInFlight;
static volatile BOOL g_asyncCaptureReady;
static volatile u16 g_asyncCaptureToken;
static u16 g_asyncNextToken = 0x5100;

/* DC2 rebuilds the software background in EFB, draws the character with GX,
 * then reads the result through five MoveImage strips.  Keep the pre-draw EFB
 * image so materialization can copy only pixels changed by the GX draws. */
static GXFrameSnapshot g_moveBaseline;

static GXDisplayMap g_activeMap;
static EfbRebuildCandidate g_rebuildCandidate;
static GXDisplayMap g_pendingPresentedMap;
static int g_pendingPresentedReady;
static uint64_t g_pendingPresentedCaptureOrder;

static uint32_t g_mapId;
static uint32_t g_prevMapId;
static uint64_t g_writeSeq;
static uint64_t g_captureOrder;

static uint64_t g_psxWriteTileEpoch[VRAM_READBACK_TILE_Y][VRAM_READBACK_TILE_X];
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
    InvalidateSnapshotsForMap(g_activeMap.map_id);
}

static void SetEfbMixedHazard(uint32_t otherMapId)
{
    InvalidateSnapshotsForMap(g_activeMap.map_id);
    InvalidateSnapshotsForMap(otherMapId);
}

static void EfbDiscardedAfterPresent(void)
{
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

static void AsyncSnapshotDrawSync(u16 token)
{
    if (g_asyncCaptureInFlight && token == g_asyncCaptureToken)
        g_asyncCaptureReady = TRUE;
}

static int PublishCompletedAsyncSnapshot(void)
{
    GXFrameSnapshot *target;
    void *reusePixels;
    int reuseSize;

    if (!g_asyncCaptureInFlight || !g_asyncCaptureReady)
        return 0;

    target = g_asyncTarget;
    if (!target)
        return 0;

    DCInvalidateRange(g_asyncSnapshot.pixels, g_asyncSnapshot.size);

    if (g_asyncSnapshot.map_id == INVALID_MAP_ID &&
        g_activeMap.map_valid &&
        g_asyncSnapshot.vram_x == g_activeMap.vram_x0 &&
        g_asyncSnapshot.vram_y == g_activeMap.vram_y0 &&
        g_asyncSnapshot.vram_w == g_activeMap.vram_x1 - g_activeMap.vram_x0 &&
        g_asyncSnapshot.vram_h == g_activeMap.vram_y1 - g_activeMap.vram_y0)
        g_asyncSnapshot.map_id = g_activeMap.map_id;

    reusePixels = target->pixels;
    reuseSize = target->size;
    *target = g_asyncSnapshot;

    memset(&g_asyncSnapshot, 0, sizeof(g_asyncSnapshot));
    g_asyncSnapshot.pixels = reusePixels;
    g_asyncSnapshot.size = reuseSize;
    g_asyncTarget = NULL;
    g_asyncCaptureReady = FALSE;
    g_asyncCaptureInFlight = FALSE;
    return 1;
}

/* Frame-end snapshots remain asynchronous.  A C0 read or MoveImage source
 * consumes psxVuw immediately, so finish a queued copy only at that read
 * boundary before exposing its pixels to the CPU. */
static int CompletePendingAsyncSnapshotForRead(void)
{
    if (!g_asyncCaptureInFlight)
        return 0;

    if (!g_asyncCaptureReady)
    {
        GX_DrawDone();
        /* GX_DrawDone guarantees completion even if the draw-sync callback
         * has not updated the notification flag before this thread resumes. */
        g_asyncCaptureReady = TRUE;
    }

    return PublishCompletedAsyncSnapshot();
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

    PublishCompletedAsyncSnapshot();

    if (!map->content_valid || g_asyncCaptureInFlight)
        return CAPTURE_REJECTED;

    if (g_asyncSnapshot.pixels == NULL || g_asyncSnapshot.size < size)
    {
        if (!AllocSnapshotBuffer(&g_asyncSnapshot, size))
            return CAPTURE_REJECTED;
    }

    g_asyncSnapshot.valid = 1;
    g_asyncSnapshot.vram_x = map->vram_x0;
    g_asyncSnapshot.vram_y = map->vram_y0;
    g_asyncSnapshot.vram_w = map->vram_x1 - map->vram_x0;
    g_asyncSnapshot.vram_h = map->vram_y1 - map->vram_y0;
    g_asyncSnapshot.viewport_x = map->viewport_x0;
    g_asyncSnapshot.viewport_y = map->viewport_y0;
    g_asyncSnapshot.viewport_w = map->viewport_x1 - map->viewport_x0;
    g_asyncSnapshot.viewport_h = map->viewport_y1 - map->viewport_y0;
    g_asyncSnapshot.ratio = map->ratio;
    g_asyncSnapshot.map_id = map->map_id;
    g_asyncSnapshot.source = source;
    g_asyncSnapshot.exactRectValid = 0;
    g_asyncSnapshot.capturedContentSeq = map->contentWriteSeq;
    g_asyncSnapshot.captureOrder = ++g_captureOrder;
    memcpy(g_asyncSnapshot.efbTile, tile, sizeof(g_asyncSnapshot.efbTile));
    g_asyncSnapshot.size = size;
    g_asyncSnapshot.texWidth = copyW;
    g_asyncSnapshot.texHeight = copyH;

    g_asyncTarget = snap;
    g_asyncCaptureReady = FALSE;
    g_asyncCaptureInFlight = TRUE;
    g_asyncCaptureToken = ++g_asyncNextToken;
    if (g_asyncCaptureToken == 0)
        g_asyncCaptureToken = ++g_asyncNextToken;

    GX_SetCopyFilter(vmode ? vmode->aa : GX_FALSE,
                     (vmode && vmode->aa) ? vmode->sample_pattern : NULL,
                     GX_FALSE, NULL);
    GX_SetTexCopySrc(0, 0, iResX, iResY);
    GX_SetTexCopyDst(copyW, copyH, GX_TF_RGB5A3, GX_FALSE);
    GX_CopyTex(MEM_K0_TO_K1(g_asyncSnapshot.pixels), GX_FALSE);
    GX_PixModeSync();
    GX_SetDrawSyncCallback(AsyncSnapshotDrawSync);
    GX_SetDrawSync(g_asyncCaptureToken);
    GX_Flush();

    RestoreDispCopyInfo();
    return CAPTURE_QUEUED;
}

static int CaptureEfbSnapshot(GXFrameSnapshot *snap,
                              const GXDisplayMap *map,
                              SnapshotSource source)
{
    if (!ActiveEfbHasFullTiles())
        return 0;

    return CaptureEfbSnapshotWithTiles(snap, map, g_efbTileState, source);
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
    int captureResult;

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

    if (g_asyncCaptureInFlight &&
        !CompletePendingAsyncSnapshotForRead())
        return 0;

    slot = OldestSnapshotSlot();
    captureResult = CaptureEfbSnapshotWithTiles(
        slot, &map, g_efbTileState, SNAPSHOT_SOURCE_PRESENTED);
    if (captureResult == CAPTURE_REJECTED)
        return 0;

    memset(g_asyncSnapshot.efbTile, 0, sizeof(g_asyncSnapshot.efbTile));
    tx0 = x >> 4;
    tx1 = (x + w - 1) >> 4;
    ty0 = y >> 4;
    ty1 = (y + h - 1) >> 4;
    for (ty = ty0; ty <= ty1; ty++)
        for (tx = tx0; tx <= tx1; tx++)
        {
            g_asyncSnapshot.efbTile[ty & 31][tx & 63].cover = EFB_TILE_FULL;
            g_asyncSnapshot.efbTile[ty & 31][tx & 63].seq = seq;
        }

    g_asyncSnapshot.exactRectValid = 1;
    g_asyncSnapshot.exact_x0 = x;
    g_asyncSnapshot.exact_y0 = y;
    g_asyncSnapshot.exact_x1 = x + w;
    g_asyncSnapshot.exact_y1 = y + h;

    if (captureResult == CAPTURE_QUEUED &&
        CompletePendingAsyncSnapshotForRead())
        captureResult = CAPTURE_COMPLETE;
    return captureResult == CAPTURE_COMPLETE;
}

static int CapturePresentedEfbSnapshot(void)
{
    GXDisplayMap map;
    GXFrameSnapshot *slot;
    int captureResult;

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
            captureResult = CaptureEfbSnapshotWithTiles(
                slot, &map, g_pendingPresentedTile,
                SNAPSHOT_SOURCE_PRESENTED);
            if (captureResult == CAPTURE_REJECTED)
                return 0;
        }
        else
        {
            captureResult = CaptureEfbSnapshotWithTiles(
                slot, &map, g_efbTileState,
                SNAPSHOT_SOURCE_PRESENTED);
            if (captureResult == CAPTURE_REJECTED)
                return 0;
        }
    }
    else
    {
        captureResult = CaptureEfbSnapshot(
            slot, &map, SNAPSHOT_SOURCE_PRESENTED);
        if (captureResult == CAPTURE_REJECTED)
            return 0;
    }

    if (captureResult == CAPTURE_QUEUED &&
        PublishCompletedAsyncSnapshot())
        captureResult = CAPTURE_COMPLETE;

#ifdef DISP_DEBUG
    if (captureResult == CAPTURE_COMPLETE)
        DebugComparePresentedToBaseline(slot);
#endif

    if (g_pendingPresentedReady)
    {
        g_pendingPresentedCaptureOrder =
            captureResult == CAPTURE_QUEUED ?
            g_asyncSnapshot.captureOrder : slot->captureOrder;
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

    if (dwActFixes & AUTO_FIX_DINO_CRISIS2)
    {
        uint64_t savedCaptureOrder = g_captureOrder;
        int captureResult;

        captureResult = CaptureEfbSnapshotWithTiles(
            &g_moveBaseline, &g_rebuildCandidate.map,
            g_rebuildCandidate.tile,
            SNAPSHOT_SOURCE_UPLOAD_REBUILD);
        if (captureResult == CAPTURE_QUEUED &&
            PublishCompletedAsyncSnapshot())
            captureResult = CAPTURE_COMPLETE;
        if (captureResult != CAPTURE_COMPLETE)
        {
            /* The old baseline belongs to an earlier rebuild.  Do not use it
             * while its replacement is still being written by GX. */
            g_moveBaseline.valid = 0;
        }

        /* The baseline is not a selectable timeline snapshot. */
        g_captureOrder = savedCaptureOrder;
    }
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
        int captureResult;

        captureResult = CaptureEfbSnapshotWithTiles(
            &g_diagBaseline, &g_rebuildCandidate.map,
            g_rebuildCandidate.tile,
            SNAPSHOT_SOURCE_UPLOAD_REBUILD);
        if (captureResult == CAPTURE_QUEUED &&
            PublishCompletedAsyncSnapshot())
            captureResult = CAPTURE_COMPLETE;
        if (captureResult == CAPTURE_COMPLETE)
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
                    "VRD BASE cycle=%u capture=%s map=%u "
                    "rect=%d,%d %dx%d\r\n",
                    g_diagCycle + 1,
                    captureResult == CAPTURE_QUEUED ? "QUEUED" : "FAIL",
                    mapId, x, y, w, h);
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

    if (g_asyncCaptureInFlight &&
        !CompletePendingAsyncSnapshotForRead())
    {
        g_lastCaptureResult = -4;
        return;
    }

    if (LIVE_SNAP()->valid &&
        LIVE_SNAP()->map_id != g_activeMap.map_id)
        RetireLiveSnapshotToPrev();

    {
     int captureResult = CaptureEfbSnapshot(
         LIVE_SNAP(), &g_activeMap, SNAPSHOT_SOURCE_CURRENT_EFB);
     if (captureResult == CAPTURE_QUEUED &&
         CompletePendingAsyncSnapshotForRead())
         captureResult = CAPTURE_COMPLETE;
     if (captureResult != CAPTURE_REJECTED)
    {
        g_activeMap.content_dirty = 0;
        g_lastCaptureResult =
            captureResult == CAPTURE_COMPLETE ? 1 : 4;
    }
    else
    {
        g_lastCaptureResult = -4;
    }
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

    if (g_asyncCaptureInFlight)
    {
        GX_DrawDone();
        g_asyncCaptureReady = TRUE;
        PublishCompletedAsyncSnapshot();
    }
    GX_SetDrawSyncCallback(NULL);

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
#ifdef DISP_DEBUG
    g_debugDrawSubmitted = 0;
    g_diagCycle = 0;
    g_diagBaselineDrawCount = 0;
    g_diagBaselinePending = 0;
    g_diagLastActiveWriteSeq = 0;
    g_diagLastExternalLogSeq = 0;
#endif

    memset(g_psxWriteTileEpoch, 0, sizeof(g_psxWriteTileEpoch));
    memset(g_efbTileState, 0, sizeof(g_efbTileState));
    memset(&g_pendingPresentedMap, 0, sizeof(g_pendingPresentedMap));
    memset(g_pendingPresentedTile, 0, sizeof(g_pendingPresentedTile));
    ResetRebuildCandidate();

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

    if (g_asyncSnapshot.pixels)
        _mem2_free(g_asyncSnapshot.pixels);
    memset(&g_asyncSnapshot, 0, sizeof(g_asyncSnapshot));
    g_asyncTarget = NULL;
    g_asyncCaptureInFlight = FALSE;
    g_asyncCaptureReady = FALSE;

    if (g_moveBaseline.pixels)
        _mem2_free(g_moveBaseline.pixels);
    memset(&g_moveBaseline, 0, sizeof(g_moveBaseline));

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

static void MergeReadbackToPsxVuwFiltered(
    int x, int y, int w, int h,
    const GXFrameSnapshot *requiredSnapshot,
    const GXFrameSnapshot *baseline)
{
    BOOL writtenTile[VRAM_READBACK_TILE_Y][VRAM_READBACK_TILE_X];
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
#endif

    PublishCompletedAsyncSnapshot();

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

    for (ty = 0; ty < VRAM_READBACK_TILE_Y; ty++)
        for (tx = 0; tx < VRAM_READBACK_TILE_X; tx++)
            if (writtenTile[ty][tx])
                InvalidateTextureArea(tx * 16, ty * 16, 16, 16);
}

static void MergeReadbackToPsxVuw(int x, int y, int w, int h)
{
    MergeReadbackToPsxVuwFiltered(x, y, w, h, NULL, NULL);
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

    PublishCompletedAsyncSnapshot();

    if (!ReadbackEnabled() || w <= 0 || h <= 0)
        return;

    mapping = ClassifyReadMapping(x, y, w, h);
    if (mapping == MAPPING_CURRENT)
        targetMapId = g_activeMap.map_id;
    else if (mapping == MAPPING_PREVIOUS)
        targetMapId = g_prevMapId;
    else
        return;

    /* The MoveImage CPU copy follows this function immediately.  Publish an
     * older frame-end/baseline producer before deciding whether a new EFB
     * capture is required and before reusing the single staging buffer. */
    if (g_asyncCaptureInFlight &&
        !CompletePendingAsyncSnapshotForRead())
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

        {
         int captureResult = CaptureEfbSnapshotWithTiles(
             slot, &map, g_pendingPresentedTile,
             SNAPSHOT_SOURCE_CURRENT_EFB);
         if (captureResult == CAPTURE_QUEUED &&
             CompletePendingAsyncSnapshotForRead())
             captureResult = CAPTURE_COMPLETE;
         captured = captureResult == CAPTURE_COMPLETE;
         if (captured)
            capturedSnapshot = slot;
         g_lastCaptureResult = captured ? 2 :
             (captureResult == CAPTURE_QUEUED ? 5 : -7);
        }
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
        SnapshotsShareSamplingMap(capturedSnapshot, &g_moveBaseline))
        baseline = &g_moveBaseline;

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
