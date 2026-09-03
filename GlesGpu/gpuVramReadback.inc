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
#include "gpuVramReadbackScope.h"

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

/* GX writes only this staging snapshot.  A completed copy is published into
 * its target on the emulation thread, so T6 never samples a buffer in flight. */
static GXFrameSnapshot g_asyncSnapshot;
static GXFrameSnapshot *g_asyncTarget;
static volatile BOOL g_asyncCaptureInFlight;
static volatile BOOL g_asyncCaptureReady;
static volatile u16 g_asyncCaptureToken;
static u16 g_asyncNextToken = 0x5100;
static unsigned int g_asyncDC2ScopeGeneration;

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
/* Conservative summary used to reject the overwhelmingly common texture
 * barrier no-op without clearing/scanning the large T6 workspace. */
static uint32_t g_t6PotentialHazardTile[VRAM_READBACK_TILE_Y][2];
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
static DC2ReadbackScope g_dc2ReadbackScope;
#ifdef DISP_DEBUG
static unsigned int g_lastMergedPixels;
static unsigned int g_lastMergedChangedPixels;
#if T6_BARRIER_DIAG
static unsigned int g_lastMergedChangedTiles;
static unsigned int g_lastMergedChangedTileBitmap[64];
static unsigned int g_lastMergedPromotedTiles;
static unsigned int g_lastMergedPromotedTileBitmap[64];
static uint64_t g_lastMergedTileSeq
    [VRAM_READBACK_TILE_Y][VRAM_READBACK_TILE_X];
static uint32_t g_lastMergedTileMapId
    [VRAM_READBACK_TILE_Y][VRAM_READBACK_TILE_X];
static unsigned char g_lastMergedTileUsed
    [VRAM_READBACK_TILE_Y][VRAM_READBACK_TILE_X];
static unsigned char g_lastMergedTileMixed
    [VRAM_READBACK_TILE_Y][VRAM_READBACK_TILE_X];
static uint64_t g_lastMergedTileSourceId
    [VRAM_READBACK_TILE_Y][VRAM_READBACK_TILE_X];
#endif
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
static unsigned int g_vramReadBarrierFastReject;
static unsigned int g_vramReadBarrierMaterialized;
static unsigned int g_vramReadBarrierUnresolved;
static unsigned int g_vramReadBarrierReentrant;
static int g_vramReadBarrierLastReason;
/* First per-tile validation failure retained for the bounded MoveImage
 * shadow log.  This is reset immediately before the read-only shadow
 * validation and never participates in production decisions. */
static int g_t6ValidationFailureTileX;
static int g_t6ValidationFailureTileY;
static uint64_t g_t6ValidationFailureBaselineSeq;
static uint64_t g_t6ValidationFailureCpuWriteEpoch;
static uint64_t g_t6ValidationFailureMaterializedEpoch;
static uint64_t g_t6ValidationFailureSnapshotSeq;
static uint64_t g_t6ValidationFailureRequiredSeq;
static uint64_t g_t6ValidationFailureSourceId;
static uint32_t g_t6ValidationFailureMapId;
static T6CpuWriteProvenanceRing g_t6CpuWriteProvenance;
/* Outcome bits for the one synchronous A0 CheckWriteUpdate call.  The
 * FinishedVRAMWrite caller attaches them to the same exact CPU sequence. */
static unsigned int g_t6A0CheckOutcomeFlags;
/* A0/GP1 transfer lifecycle.  The provenance ring snapshots these fields at
 * finish, avoiding unbounded per-transfer SD logging before MoveImage tells
 * us which exact CPU epoch matters. */
static unsigned int g_t6A0TransferGeneration;
static unsigned int g_t6A0FinishSerial;
static unsigned int g_t6A0DmaSerial;
static unsigned int g_t6A0ArmDmaSerial;
static int g_t6A0TransferArmed;
static int g_t6A0ArmX;
static int g_t6A0ArmY;
static int g_t6A0ArmW;
static int g_t6A0ArmH;
static int g_t6A0LastDmaMode;
static int g_t6A0LastDmaOldWriteMode;
static int g_t6A0LastDmaNewWriteMode;
static unsigned int g_t6A0UnarmedWriteCalls;
/* Bounded CPU-newer negative-probe lifecycle.  Keep these beside the other
 * DIAG counters so ResetVramReadbackState() starts every GPU session fresh. */
static unsigned int g_t6CpuNewerProbeRelevant;
static unsigned int g_t6CpuNewerProbeCandidates;
static unsigned int g_t6CpuNewerProbePasses;
/* Bounded C0 lifecycle probe.  This is observation-only: it must not alter
 * readback state or payload counters.  The active serial links the command,
 * merge, payload progress and FinishedVRAMRead records. */
static unsigned int g_t6C0TransferSerial;
static unsigned int g_t6C0ActiveSerial;
static unsigned int g_t6C0ReadCalls;
static unsigned int g_t6C0LifecycleLogs;
static unsigned int g_t6C0LastFinishedSerial;
static unsigned int g_t6C0ReturnLogs;
static unsigned int g_vramReadBarrierCopyTex;
static unsigned int g_vramReadBarrierCopyTexTotal;
static unsigned int g_vramReadBarrierChangedTiles;
static unsigned int g_vramReadBarrierInvalidateRuns;
/* T6 performance telemetry. Per-call fields are reset by the materialize
 * core; cumulative fields are emitted at power-of-two call milestones and
 * on readback reset, keeping file I/O bounded while still exposing stalls. */
static unsigned int g_vramReadBarrierCaptureUs;
static unsigned int g_vramReadBarrierWriteUs;
static unsigned int g_vramReadBarrierInvalUs;
static unsigned int g_vramReadBarrierPaletteChecks;
static unsigned int g_vramReadBarrierInvalidatedEntries;
static unsigned int g_vramReadBarrierSampleFastTiles;
static unsigned int g_vramReadBarrierSampleFallbackTiles;
static unsigned int g_vramReadBarrierSampleNoDeltaTiles;
static uint64_t g_vramReadBarrierUsTotal;
static uint64_t g_vramReadBarrierCaptureUsTotal;
static uint64_t g_vramReadBarrierWriteUsTotal;
static uint64_t g_vramReadBarrierInvalUsTotal;
static uint64_t g_vramReadBarrierChangedTilesTotal;
static uint64_t g_vramReadBarrierInvalidateRunsTotal;
static uint64_t g_vramReadBarrierPaletteChecksTotal;
static uint64_t g_vramReadBarrierInvalidatedEntriesTotal;
static uint64_t g_vramReadBarrierSampleFastTilesTotal;
static uint64_t g_vramReadBarrierSampleFallbackTilesTotal;
static uint64_t g_vramReadBarrierSampleNoDeltaTilesTotal;
static unsigned int g_vramReadBarrierUsMax;
static unsigned int g_vramReadBarrierCaptureUsMax;
static unsigned int g_vramReadBarrierSlowCalls;
static unsigned int g_vramReadBarrierSuccessLogs;
static unsigned int g_vramReadBarrierReasonLogs[T6_REASON_COUNT];
/* Rebuild baselines are correctness snapshots taken outside the generic
 * texture barrier.  Track them separately so a full-EFB CopyTex stall cannot
 * be mistaken for texture lookup or invalidation cost. */
static unsigned int g_t6RebuildBaselineCalls;
static unsigned int g_t6RebuildBaselineCaptures;
static unsigned int g_t6RebuildBaselineFailures;
static uint64_t g_t6RebuildBaselineUsTotal;
static unsigned int g_t6RebuildBaselineUsMax;
static unsigned int g_vramMoveShadowCalls;
static unsigned int g_vramMoveShadowBeforeNoAction;
static unsigned int g_vramMoveShadowBeforeMaterialized;
static unsigned int g_vramMoveShadowBeforeUnresolved;
static unsigned int g_vramMoveTakeCalls;
static unsigned int g_vramMoveTakeNoAction;
static unsigned int g_vramMoveTakeMaterialized;
static unsigned int g_vramMoveTakeUnresolved;
static unsigned int g_vramMoveTakePostHazard;
static unsigned int g_vramMoveTakeLogs;
/* Keep the ~53 KiB generic before/post evidence out of MEM1 .bss.  It is allocated
 * lazily from MEM2 at the first MoveImage, so nothing before that command can
 * be affected by its former BSS layout shift.  Guards detect overwrites while
 * preserving the existing workspace + busy contract. */
#define T6_MOVE_DIAG_GUARD_BEFORE 0x54364247u
#define T6_MOVE_DIAG_GUARD_AFTER  0x54364147u
typedef struct T6MoveTakeDiagAllocation
{
    uint32_t guardBefore;
    T6MoveTakeDiagWorkspace workspace;
    uint32_t guardAfter;
} T6MoveTakeDiagAllocation;
static T6MoveTakeDiagAllocation *g_t6MoveTakeDiagAllocation;
#define g_t6MoveTakeDiag (g_t6MoveTakeDiagAllocation->workspace)

static int T6MoveTakeDiagCanaryStatus(void)
{
    if (g_t6MoveTakeDiagAllocation == NULL)
        return 0;
    if (g_t6MoveTakeDiagAllocation->guardBefore !=
            T6_MOVE_DIAG_GUARD_BEFORE ||
        g_t6MoveTakeDiagAllocation->guardAfter !=
            T6_MOVE_DIAG_GUARD_AFTER)
        return -2;
    return 1;
}

static int T6MoveTakeDiagEnsureWorkspace(void)
{
    if (g_t6MoveTakeDiagAllocation == NULL)
    {
        g_t6MoveTakeDiagAllocation =
            (T6MoveTakeDiagAllocation *)_mem2_memalign(
                32, sizeof(*g_t6MoveTakeDiagAllocation));
        if (g_t6MoveTakeDiagAllocation == NULL)
        {
            g_t6DiagWorkspaceStatus = -1;
            return 0;
        }
        g_t6MoveTakeDiagAllocation->guardBefore =
            T6_MOVE_DIAG_GUARD_BEFORE;
        g_t6MoveTakeDiagAllocation->guardAfter =
            T6_MOVE_DIAG_GUARD_AFTER;
        memset(&g_t6MoveTakeDiag, 0, sizeof(g_t6MoveTakeDiag));
    }
    g_t6DiagWorkspaceStatus = T6MoveTakeDiagCanaryStatus();
    return g_t6DiagWorkspaceStatus == 1;
}
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
#if defined(DISP_DEBUG) && defined(VRAM_CONTENT_DIAG)
static void DebugComparePresentedToBaseline(const GXFrameSnapshot *snap);
#endif

static BOOL ReadbackEnabled(void)
{
	return (dwActFixes & AUTO_FIX_VRAM_READBACK) != 0;
}

/* Keep ownership/epoch tracking enabled for the whole game, but restrict
 * DC2's expensive EFB copies and materialization scans to the proven
 * underwater command window.  Other readback titles retain their existing
 * behaviour. */
static BOOL ReadbackMaterializationEnabled(void)
{
    return ReadbackEnabled() &&
           DC2ReadbackScopeAllowsWork(
               (dwActFixes & AUTO_FIX_DINO_CRISIS2) != 0,
               &g_dc2ReadbackScope);
}

static int PublishCompletedAsyncSnapshot(void);

static void InvalidateDC2ScopedSnapshots(void)
{
    int i;

    if (!(dwActFixes & AUTO_FIX_DINO_CRISIS2))
        return;

    for (i = 0; i < 2; i++)
        g_snapshot[i].valid = 0;
    g_rebuildBaseline.valid = 0;
    g_pendingPresentedCaptureOrder = 0;
}

static void ObserveDC2ReadbackC0(int x, int y, int w, int h)
{
    int wasActive;

    if (!(dwActFixes & AUTO_FIX_DINO_CRISIS2) ||
        !DC2ReadbackScopeIsTriggerC0(x, y, w, h))
        return;

    wasActive = DC2ReadbackScopeActive(&g_dc2ReadbackScope);
    if (!wasActive)
        InvalidateDC2ScopedSnapshots();
    DC2ReadbackScopeArm(&g_dc2ReadbackScope);

#ifdef DISP_DEBUG
    if (!wasActive)
    {
        sprintf(txtbuffer,
                "VRB DC2 SCOPE arm c0=%d,%d %dx%d hold=%u\r\n",
                x, y, w, h, g_dc2ReadbackScope.holdFrames);
        writeLogFile(txtbuffer);
    }
#endif
}

static void ObserveDC2ReadbackMove(
    int sourceX, int sourceY, int destinationX, int destinationY,
    int w, int h)
{
    if (!(dwActFixes & AUTO_FIX_DINO_CRISIS2) ||
        !DC2ReadbackScopeIsUnderwaterMove(
            sourceX, sourceY, destinationX, destinationY, w, h))
        return;

    DC2ReadbackScopeArm(&g_dc2ReadbackScope);
}

static void AdvanceDC2ReadbackScopeAfterPresent(void)
{
    if (!(dwActFixes & AUTO_FIX_DINO_CRISIS2))
        return;

    /* Drain a ready copy even while the window is closed.  Generation
     * validation in PublishCompletedAsyncSnapshot() retires a late result
     * instead of allowing it to block the next underwater window. */
    PublishCompletedAsyncSnapshot();

    if (!DC2ReadbackScopeAdvanceFrame(&g_dc2ReadbackScope))
        return;

    /* Buffers stay allocated for reuse.  Only their selectable contents are
     * retired, so leaving the underwater scene neither frees MEM2 every time
     * nor permits a stale water snapshot to affect later texture reads. */
    InvalidateDC2ScopedSnapshots();
#ifdef DISP_DEBUG
    sprintf(txtbuffer, "VRB DC2 SCOPE expire\r\n");
    writeLogFile(txtbuffer);
#endif
}

#if defined(DISP_DEBUG) && defined(VRAM_CONTENT_DIAG)
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
#endif /* DISP_DEBUG && VRAM_CONTENT_DIAG */

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

#ifdef DISP_DEBUG
    if ((dwActFixes & AUTO_FIX_DINO_CRISIS2) &&
        g_dc2PauseDiagDrawBudget != 0 &&
        g_dc2PauseDiagDrawLogs < 4096)
    {
        g_dc2PauseDiagDrawBudget--;
        g_dc2PauseDiagDrawLogs++;
        sprintf(txtbuffer,
                "TRB DC2 DRAW event=%u barrier=%u n=%d off=%d "
                "xy=%d,%d/%d,%d/%d,%d/%d,%d "
                "bbox=%d,%d-%d,%d uv=%u,%u/%u,%u/%u,%u/%u,%u "
                "page=%d mode=%d map=%u pending=%d seq=%u\r\n",
                g_dc2PauseDiagEventSerial, g_dc2PauseDiagBarrierSerial,
                count, addDrawOffset,
                lx[0], ly[0], lx[1], ly[1],
                lx[2], ly[2], lx[3], ly[3],
                x0, y0, x1, y1,
                gl_ux[0], gl_vy[0], gl_ux[1], gl_vy[1],
                gl_ux[2], gl_vy[2], gl_ux[3], gl_vy[3],
                GlobalTexturePage, GlobalTextTP,
                ctx->mapId, usePending,
                (unsigned int)g_activeMap.contentWriteSeq);
        writeLogFile(txtbuffer);
    }
#endif
}

static uint64_t BeginCommandWrite(void)
{
    if (!ReadbackEnabled())
        return 0;

    return ++g_writeSeq;
}

static void MarkCpuVramWriteWithSeqKind(
    uint64_t seq, int x, int y, int w, int h,
    int kind, unsigned int flags)
{
    if (!ReadbackEnabled())
        return;

    if (w <= 0 || h <= 0)
        return;

#if T6_BARRIER_DIAG
    T6CpuWriteProvenanceRecord(&g_t6CpuWriteProvenance, seq, kind,
                               x, y, w, h, flags);
#else
    (void)kind;
    (void)flags;
#endif

    while (w > 0)
    {
        int x1 = (x + w <= 1024) ? x + w : 1024;
        int tx0 = x >> 4, tx1 = (x1 - 1) >> 4;
        int ty0 = y >> 4, ty1 = (y + h - 1) >> 4;
        int ty, tx;

        for (ty = ty0; ty <= ty1; ty++)
            for (tx = tx0; tx <= tx1; tx++)
            {
                g_psxWriteTileEpoch[ty & 31][tx & 63] = seq;
                /* A sequence allocated at the current timeline head is at
                 * least as new as every tracked EFB/snapshot source. */
                if (seq == g_writeSeq)
                    g_t6PotentialHazardTile[ty & 31][(tx & 63) >> 5] &=
                        ~(1u << (tx & 31));
            }

        w -= x1 - x;
        x = 0;
    }
}

static uint64_t MarkCpuVramWriteKind(
    int x, int y, int w, int h, int kind, unsigned int flags)
{
    uint64_t seq;

    if (!ReadbackEnabled())
        return 0;

    seq = ++g_writeSeq;
    MarkCpuVramWriteWithSeqKind(seq, x, y, w, h, kind, flags);
    return seq;
}

static void MarkPotentialEfbRect(uint64_t seq,
                                 int x0, int y0, int x1, int y1)
{
    int tx0, tx1, ty0, ty1, ty, tx;

    if (seq == 0 || x1 <= x0 || y1 <= y0)
        return;

    tx0 = x0 >> 4;
    tx1 = (x1 - 1) >> 4;
    ty0 = y0 >> 4;
    ty1 = (y1 - 1) >> 4;
    for (ty = ty0; ty <= ty1; ty++)
        for (tx = tx0; tx <= tx1; tx++)
            if (seq > PsxVuwColorTileEpoch(
                    g_psxWriteTileEpoch[ty & 31][tx & 63],
                    g_psxMaterializedColorTileEpoch[ty & 31][tx & 63]))
                g_t6PotentialHazardTile[ty & 31][(tx & 63) >> 5] |=
                    1u << (tx & 31);
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
                if (seq > PsxVuwColorTileEpoch(
                        g_psxWriteTileEpoch[ty & 31][tx & 63],
                        g_psxMaterializedColorTileEpoch[ty & 31][tx & 63]))
                    g_t6PotentialHazardTile[ty & 31][(tx & 63) >> 5] |=
                        1u << (tx & 31);
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

/* A capture replaces a selectable slot with metadata for the whole sampled
 * EFB mapping.  Merge every full/newer tile into the cheap prefilter summary
 * before the slot can be consumed.  Baseline/diagnostic snapshots are not
 * timeline sources and must not affect the production summary. */
static void MergeSelectableSnapshotPotential(const GXFrameSnapshot *snap)
{
    int tx, ty;

    if (snap == NULL || !snap->valid ||
        (snap != &g_snapshot[0] && snap != &g_snapshot[1]))
        return;

    for (ty = 0; ty < VRAM_READBACK_TILE_Y; ty++)
        for (tx = 0; tx < VRAM_READBACK_TILE_X; tx++)
        {
            const EfbTileState *efb = &snap->efbTile[ty][tx];

            T6PotentialMergeSnapshotTile(
                g_t6PotentialHazardTile, tx, ty,
                efb->cover == EFB_TILE_FULL, efb->seq,
                g_psxWriteTileEpoch[ty][tx],
                g_psxMaterializedColorTileEpoch[ty][tx]);
        }
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
        MarkPotentialEfbRect(seq, ctx->x0, ctx->y0, ctx->x1, ctx->y1);

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
            MarkPotentialEfbRect(seq, ctx->x0, ctx->y0,
                                ctx->x1, ctx->y1);
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
    int scopedCaptureCurrent;

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
    scopedCaptureCurrent = ReadbackEnabled() &&
        DC2ReadbackScopeCaptureIsCurrent(
            (dwActFixes & AUTO_FIX_DINO_CRISIS2) != 0,
            &g_dc2ReadbackScope, g_asyncDC2ScopeGeneration);
    if (scopedCaptureCurrent)
        MergeSelectableSnapshotPotential(target);
    else
        target->valid = 0;

    memset(&g_asyncSnapshot, 0, sizeof(g_asyncSnapshot));
    g_asyncSnapshot.pixels = reusePixels;
    g_asyncSnapshot.size = reuseSize;
    g_asyncTarget = NULL;
    g_asyncCaptureReady = FALSE;
    g_asyncCaptureInFlight = FALSE;
    g_asyncDC2ScopeGeneration = 0;
    return 1;
}

/* A GX copy may stay asynchronous while it is only producing a snapshot for
 * a later frame.  A CPU VRAM read is different: its caller consumes psxVuw
 * before returning, so allowing a queued copy to escape this point exposes
 * stale pixels.  Wait only at that consumption boundary, then publish through
 * the normal generation/map validation path. */
static int CompletePendingAsyncSnapshotForRead(void)
{
    if (!g_asyncCaptureInFlight)
        return 0;

    if (!g_asyncCaptureReady)
    {
        GX_DrawDone();
        /* GX_DrawDone guarantees that the preceding draw-sync token and copy
         * have completed.  Set the flag explicitly as the callback is only a
         * non-blocking notification and need not run before we resume. */
        g_asyncCaptureReady = TRUE;
    }

    return PublishCompletedAsyncSnapshot();
}

static BOOL CanCaptureActiveEfb(void)
{
    if (!ReadbackMaterializationEnabled())
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

    if (!ReadbackMaterializationEnabled() ||
        !map->content_valid || g_asyncCaptureInFlight)
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
    g_asyncDC2ScopeGeneration =
        (dwActFixes & AUTO_FIX_DINO_CRISIS2) ?
            g_dc2ReadbackScope.generation : 0;
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

static void EstablishRebuildBaseline(
    const GXDisplayMap *map,
    EfbTileState tile[VRAM_READBACK_TILE_Y][VRAM_READBACK_TILE_X],
    SnapshotSource source)
{
    uint64_t savedCaptureOrder = g_captureOrder;
#if T6_BARRIER_DIAG
    uint64_t ticksStart = (uint64_t)gettime();
    unsigned int us;
    int captureResult;

    g_t6RebuildBaselineCalls++;
    captureResult = CaptureEfbSnapshotWithTiles(
        &g_rebuildBaseline, map, tile, source);
    if (captureResult == CAPTURE_QUEUED &&
        PublishCompletedAsyncSnapshot())
        captureResult = CAPTURE_COMPLETE;
    us = (unsigned int)ticks_to_microsecs(
        (uint64_t)gettime() - ticksStart);
    g_t6RebuildBaselineUsTotal += us;
    if (us > g_t6RebuildBaselineUsMax)
        g_t6RebuildBaselineUsMax = us;
    if (captureResult == CAPTURE_COMPLETE)
        g_t6RebuildBaselineCaptures++;
    else
        g_t6RebuildBaselineFailures++;
#else
    int captureResult = CaptureEfbSnapshotWithTiles(
        &g_rebuildBaseline, map, tile, source);
    if (captureResult == CAPTURE_QUEUED &&
        PublishCompletedAsyncSnapshot())
        captureResult = CAPTURE_COMPLETE;
#endif

    if (captureResult != CAPTURE_COMPLETE)
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

    /* C0 returns these pixels immediately.  Drain an older asynchronous
     * producer before reusing the single staging buffer. */
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
    MarkPotentialEfbRect(seq, x, y, x + w, y + h);

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

#if defined(DISP_DEBUG) && defined(VRAM_CONTENT_DIAG)
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

#if defined(DISP_DEBUG) && defined(VRAM_CONTENT_DIAG)
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
#endif
#ifdef DISP_DEBUG
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
    int captureResult;

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

    /* Unlike the frame-end producer, C0 consumes the captured pixels in this
     * call.  Finish any prior producer so the staging slot can be reused. */
    if (g_asyncCaptureInFlight &&
        !CompletePendingAsyncSnapshotForRead())
    {
        g_lastCaptureResult = -4;
        return;
    }

    if (LIVE_SNAP()->valid &&
        LIVE_SNAP()->map_id != g_activeMap.map_id)
        RetireLiveSnapshotToPrev();

    captureResult = CaptureEfbSnapshot(
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

    if (g_asyncCaptureInFlight)
    {
        GX_DrawDone();
        g_asyncCaptureReady = TRUE;
        PublishCompletedAsyncSnapshot();
    }
    GX_SetDrawSyncCallback(NULL);

#if T6_BARRIER_DIAG
    sprintf(txtbuffer,
            "TRB STATS calls=%u noAction=%u fastReject=%u materialized=%u unresolved=%u "
            "reentrant=%u copyTexTotal=%u changedTotal=%llu "
            "invalRunsTotal=%llu totalUs=%llu maxUs=%u slow=%u "
            "captureUs=%llu captureMaxUs=%u writeUs=%llu invalUs=%llu "
            "sampleFast=%llu sampleFallback=%llu sampleNoDelta=%llu "
            "palChecks=%llu invalidated=%llu stdHit=%u stdUp=%u "
            "wndHit=%u wndUp=%u\r\n",
            g_vramReadBarrierCalls, g_vramReadBarrierNoAction,
            g_vramReadBarrierFastReject,
            g_vramReadBarrierMaterialized, g_vramReadBarrierUnresolved,
            g_vramReadBarrierReentrant, g_vramReadBarrierCopyTexTotal,
            (unsigned long long)g_vramReadBarrierChangedTilesTotal,
            (unsigned long long)g_vramReadBarrierInvalidateRunsTotal,
            (unsigned long long)g_vramReadBarrierUsTotal,
            g_vramReadBarrierUsMax, g_vramReadBarrierSlowCalls,
            (unsigned long long)g_vramReadBarrierCaptureUsTotal,
            g_vramReadBarrierCaptureUsMax,
            (unsigned long long)g_vramReadBarrierWriteUsTotal,
            (unsigned long long)g_vramReadBarrierInvalUsTotal,
            (unsigned long long)g_vramReadBarrierSampleFastTilesTotal,
            (unsigned long long)g_vramReadBarrierSampleFallbackTilesTotal,
            (unsigned long long)g_vramReadBarrierSampleNoDeltaTilesTotal,
            (unsigned long long)g_vramReadBarrierPaletteChecksTotal,
            (unsigned long long)g_vramReadBarrierInvalidatedEntriesTotal,
            g_textureStandardCacheHits, g_textureStandardUploads,
            g_textureWindowCacheHits, g_textureWindowUploads);
    writeLogFile(txtbuffer);
    sprintf(txtbuffer,
            "TRB MOVE STATS calls=%u beforeNoAction=%u "
            "beforeMaterialized=%u beforeUnresolved=%u "
            "takeNoAction=%u takeMaterialized=%u takeUnresolved=%u "
            "takePostHazard=%u\r\n",
            g_vramMoveShadowCalls, g_vramMoveShadowBeforeNoAction,
            g_vramMoveShadowBeforeMaterialized,
            g_vramMoveShadowBeforeUnresolved,
            g_vramMoveTakeNoAction, g_vramMoveTakeMaterialized,
            g_vramMoveTakeUnresolved, g_vramMoveTakePostHazard);
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
    g_dc2ReadbackScope.holdFrames = 0;
    g_dc2ReadbackScope.generation = 0;
    g_vramReadBarrierDepth = 0;
    g_t6WorkspaceDepth = 0;
#ifdef DISP_DEBUG
    g_dc2PauseDiagBarrierSerial = 0;
    g_dc2PauseDiagEventSerial = 0;
    g_dc2PauseDiagEventLogs = 0;
    g_dc2PauseDiagTextureLogs = 0;
    g_dc2PauseDiagDrawLogs = 0;
    g_dc2PauseDiagInvalidateLogs = 0;
    g_dc2PauseDiagDrawBudget = 0;
    g_dc2PauseFeedbackSkips = 0;
    g_dc2PauseDiagLastResult = VRAM_FRESH_NO_ACTION;
    g_dc2PauseDiagLastChanged = 0;
    g_debugDrawSubmitted = 0;
    g_diagCycle = 0;
    g_diagBaselineDrawCount = 0;
    g_diagBaselinePending = 0;
    g_diagLastActiveWriteSeq = 0;
    g_diagLastExternalLogSeq = 0;
#if T6_BARRIER_DIAG
    g_vramReadBarrierCalls = 0;
    g_vramReadBarrierNoAction = 0;
    g_vramReadBarrierFastReject = 0;
    g_vramReadBarrierMaterialized = 0;
    g_vramReadBarrierUnresolved = 0;
    g_vramReadBarrierReentrant = 0;
    g_vramReadBarrierLastReason = T6_REASON_NONE;
    T6CopyTexBeginCall(&g_vramReadBarrierCopyTex);
    g_vramReadBarrierCopyTexTotal = 0;
    g_vramReadBarrierChangedTiles = 0;
    g_vramReadBarrierInvalidateRuns = 0;
    g_vramReadBarrierCaptureUs = 0;
    g_vramReadBarrierWriteUs = 0;
    g_vramReadBarrierInvalUs = 0;
    g_vramReadBarrierPaletteChecks = 0;
    g_vramReadBarrierInvalidatedEntries = 0;
    g_vramReadBarrierSampleFastTiles = 0;
    g_vramReadBarrierSampleFallbackTiles = 0;
    g_vramReadBarrierSampleNoDeltaTiles = 0;
    g_vramReadBarrierUsTotal = 0;
    g_vramReadBarrierCaptureUsTotal = 0;
    g_vramReadBarrierWriteUsTotal = 0;
    g_vramReadBarrierInvalUsTotal = 0;
    g_vramReadBarrierChangedTilesTotal = 0;
    g_vramReadBarrierInvalidateRunsTotal = 0;
    g_vramReadBarrierPaletteChecksTotal = 0;
    g_vramReadBarrierInvalidatedEntriesTotal = 0;
    g_vramReadBarrierSampleFastTilesTotal = 0;
    g_vramReadBarrierSampleFallbackTilesTotal = 0;
    g_vramReadBarrierSampleNoDeltaTilesTotal = 0;
    g_vramReadBarrierUsMax = 0;
    g_vramReadBarrierCaptureUsMax = 0;
    g_vramReadBarrierSlowCalls = 0;
    g_vramReadBarrierSuccessLogs = 0;
    memset(g_vramReadBarrierReasonLogs, 0,
           sizeof(g_vramReadBarrierReasonLogs));
    g_t6RebuildBaselineCalls = 0;
    g_t6RebuildBaselineCaptures = 0;
    g_t6RebuildBaselineFailures = 0;
    g_t6RebuildBaselineUsTotal = 0;
    g_t6RebuildBaselineUsMax = 0;
    g_vramMoveShadowCalls = 0;
    g_vramMoveShadowBeforeNoAction = 0;
    g_vramMoveShadowBeforeMaterialized = 0;
    g_vramMoveShadowBeforeUnresolved = 0;
    g_vramMoveTakeCalls = 0;
    g_vramMoveTakeNoAction = 0;
    g_vramMoveTakeMaterialized = 0;
    g_vramMoveTakeUnresolved = 0;
    g_vramMoveTakePostHazard = 0;
    g_vramMoveTakeLogs = 0;
#if T6_BARRIER_DIAG
    /* 已分配的 MEM2 workspace 保留复用并复位 busy/guards。 */
    if (g_t6MoveTakeDiagAllocation != NULL)
    {
        g_t6MoveTakeDiag.busy = 0;
        g_t6MoveTakeDiagAllocation->guardBefore =
            T6_MOVE_DIAG_GUARD_BEFORE;
        g_t6MoveTakeDiagAllocation->guardAfter =
            T6_MOVE_DIAG_GUARD_AFTER;
    }
    g_t6DiagWorkspaceStatus = T6MoveTakeDiagCanaryStatus();
#endif
    T6CpuWriteProvenanceInit(&g_t6CpuWriteProvenance);
    g_t6A0CheckOutcomeFlags = 0;
    g_t6A0TransferGeneration = 0;
    g_t6A0FinishSerial = 0;
    g_t6A0DmaSerial = 0;
    g_t6A0ArmDmaSerial = 0;
    g_t6A0TransferArmed = 0;
    g_t6A0ArmX = 0;
    g_t6A0ArmY = 0;
    g_t6A0ArmW = 0;
    g_t6A0ArmH = 0;
    g_t6A0LastDmaMode = 0;
    g_t6A0LastDmaOldWriteMode = 0;
    g_t6A0LastDmaNewWriteMode = 0;
    g_t6A0UnarmedWriteCalls = 0;
    g_t6CpuNewerProbeRelevant = 0;
    g_t6CpuNewerProbeCandidates = 0;
    g_t6CpuNewerProbePasses = 0;
    g_t6C0TransferSerial = 0;
    g_t6C0ActiveSerial = 0;
    g_t6C0ReadCalls = 0;
    g_t6C0LifecycleLogs = 0;
    g_t6C0LastFinishedSerial = 0;
    g_t6C0ReturnLogs = 0;
#endif
#endif

    memset(g_psxWriteTileEpoch, 0, sizeof(g_psxWriteTileEpoch));
    memset(g_psxMaterializedColorTileEpoch, 0,
           sizeof(g_psxMaterializedColorTileEpoch));
    memset(g_t6PotentialHazardTile, 0,
           sizeof(g_t6PotentialHazardTile));
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
#if T6_BARRIER_DIAG
    g_lastMergedChangedTiles = 0;
    memset(g_lastMergedChangedTileBitmap, 0,
           sizeof(g_lastMergedChangedTileBitmap));
    g_lastMergedPromotedTiles = 0;
    memset(g_lastMergedPromotedTileBitmap, 0,
           sizeof(g_lastMergedPromotedTileBitmap));
    memset(g_lastMergedTileUsed, 0, sizeof(g_lastMergedTileUsed));
    memset(g_lastMergedTileSeq, 0, sizeof(g_lastMergedTileSeq));
    memset(g_lastMergedTileMapId, 0, sizeof(g_lastMergedTileMapId));
    memset(g_lastMergedTileMixed, 0, sizeof(g_lastMergedTileMixed));
    memset(g_lastMergedTileSourceId, 0,
           sizeof(g_lastMergedTileSourceId));
#endif
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
    g_asyncDC2ScopeGeneration = 0;

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

#if defined(DISP_DEBUG) && defined(VRAM_CONTENT_DIAG)
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
#endif /* DISP_DEBUG && VRAM_CONTENT_DIAG */

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

/* Dino Crisis 2 pause-feedback compatibility exception.
 *
 * This is deliberately stricter than a page-number or rectangle-only hack.
 * A read is suppressed only when all of the following describe the observed
 * four-quad page flip:
 *   - standard RGB15 texture data overlapping the current display map;
 *   - an axis-aligned 160x120 source/destination quadrant (up to the game's
 *     two-pixel inset);
 *   - destination coordinates wholly inside the distinct pending display
 *     page which is about to become visible.
 *
 * The test runs before psxVuw materialization.  Skipping invalidation alone
 * would be insufficient because a later cache miss could still decode the
 * recursively modified CPU-side image. */
static BOOL ShouldSkipDC2PauseFeedbackMaterialize(
    const VramReadDependency *dependency, int pageid, int textureMode)
{
    const VramRect *source;
    int ux0, ux1, vy0, vy1;
    int dx[4], dy[4];
    int dx0, dx1, dy0, dy1;
    int activeW, activeH, pendingW, pendingH;
    int i;

    if (!(dwActFixes & AUTO_FIX_DINO_CRISIS2) ||
        dependency == NULL || dependency->kind != VRAM_READ_TEXTURE ||
        textureMode != 2 || GlobalTextIL ||
        pageid < 0 || pageid > 31 || dependency->count != 2)
        return FALSE;

    source = &dependency->rect[0];
    if (source->x0 != dependency->rect[1].x0 ||
        source->y0 != dependency->rect[1].y0 ||
        source->x1 != dependency->rect[1].x1 ||
        source->y1 != dependency->rect[1].y1 ||
        source->x1 - source->x0 != 161 ||
        source->y1 - source->y0 != 121 ||
        ClassifyReadMapping(source->x0, source->y0,
                            source->x1 - source->x0,
                            source->y1 - source->y0) != MAPPING_CURRENT)
        return FALSE;

    if (!g_activeMap.map_valid || !g_pendingPresentedReady ||
        !g_pendingPresentedMap.map_valid ||
        (g_pendingPresentedMap.vram_x0 == g_activeMap.vram_x0 &&
         g_pendingPresentedMap.vram_y0 == g_activeMap.vram_y0 &&
         g_pendingPresentedMap.vram_x1 == g_activeMap.vram_x1 &&
         g_pendingPresentedMap.vram_y1 == g_activeMap.vram_y1))
        return FALSE;

    activeW = g_activeMap.vram_x1 - g_activeMap.vram_x0;
    activeH = g_activeMap.vram_y1 - g_activeMap.vram_y0;
    pendingW = g_pendingPresentedMap.vram_x1 -
               g_pendingPresentedMap.vram_x0;
    pendingH = g_pendingPresentedMap.vram_y1 -
               g_pendingPresentedMap.vram_y0;
    if (activeW != 320 || activeH != 240 ||
        pendingW != 320 || pendingH != 240)
        return FALSE;

    ux0 = ux1 = gl_ux[0];
    vy0 = vy1 = gl_vy[0];
    dx0 = dx1 = lx0 + PSXDisplay.DrawOffset.x;
    dy0 = dy1 = ly0 + PSXDisplay.DrawOffset.y;
    dx[0] = dx0;
    dy[0] = dy0;
    dx[1] = lx1 + PSXDisplay.DrawOffset.x;
    dy[1] = ly1 + PSXDisplay.DrawOffset.y;
    dx[2] = lx2 + PSXDisplay.DrawOffset.x;
    dy[2] = ly2 + PSXDisplay.DrawOffset.y;
    dx[3] = lx3 + PSXDisplay.DrawOffset.x;
    dy[3] = ly3 + PSXDisplay.DrawOffset.y;

    for (i = 1; i < 4; i++)
    {
        if ((int)gl_ux[i] < ux0) ux0 = gl_ux[i];
        if ((int)gl_ux[i] > ux1) ux1 = gl_ux[i];
        if ((int)gl_vy[i] < vy0) vy0 = gl_vy[i];
        if ((int)gl_vy[i] > vy1) vy1 = gl_vy[i];
        if (dx[i] < dx0) dx0 = dx[i];
        if (dx[i] > dx1) dx1 = dx[i];
        if (dy[i] < dy0) dy0 = dy[i];
        if (dy[i] > dy1) dy1 = dy[i];
    }

    if (ux1 - ux0 != 160 || vy1 - vy0 != 120 ||
        dx1 - dx0 < 158 || dx1 - dx0 > 160 ||
        dy1 - dy0 < 118 || dy1 - dy0 > 120)
        return FALSE;

    /* Reject triangles and arbitrary quads which happen to share the same
     * bounding box: every vertex must be one of the four rectangle corners. */
    for (i = 0; i < 4; i++)
        if ((dx[i] != dx0 && dx[i] != dx1) ||
            (dy[i] != dy0 && dy[i] != dy1))
            return FALSE;

    if (dx0 < g_pendingPresentedMap.vram_x0 ||
        dy0 < g_pendingPresentedMap.vram_y0 ||
        dx1 > g_pendingPresentedMap.vram_x1 ||
        dy1 > g_pendingPresentedMap.vram_y1)
        return FALSE;

    /* Each command covers one half-width by half-height quadrant.  Requiring
     * an edge near both the display boundary and its midpoint prevents large
     * ordinary sprites from being mistaken for the pause page-flip sequence. */
    if (!((dx0 - g_pendingPresentedMap.vram_x0 <= 2 &&
           dx1 - g_pendingPresentedMap.vram_x0 == 160) ||
          (dx0 - g_pendingPresentedMap.vram_x0 == 160 &&
           g_pendingPresentedMap.vram_x1 - dx1 <= 2)) ||
        !((dy0 - g_pendingPresentedMap.vram_y0 <= 2 &&
           dy1 - g_pendingPresentedMap.vram_y0 == 120) ||
          (dy0 - g_pendingPresentedMap.vram_y0 == 120 &&
           g_pendingPresentedMap.vram_y1 - dy1 <= 2)))
        return FALSE;

#ifdef DISP_DEBUG
    g_dc2PauseDiagLastResult = VRAM_FRESH_NO_ACTION;
    g_dc2PauseDiagLastChanged = 0;
    g_dc2PauseFeedbackSkips++;
    if (g_dc2PauseDiagTextureLogs < 4096)
    {
        g_dc2PauseDiagTextureLogs++;
        sprintf(txtbuffer,
                "TRB DC2 FB_SKIP count=%u barrier=%u "
                "page=%d src=%d,%d-%d,%d "
                "dst=%d,%d-%d,%d active=%u pending=%u\r\n",
                g_dc2PauseFeedbackSkips,
                g_dc2PauseDiagBarrierSerial, pageid,
                source->x0, source->y0, source->x1, source->y1,
                dx0, dy0, dx1 + 1, dy1 + 1,
                g_activeMap.map_id, g_pendingPresentedMap.map_id);
        writeLogFile(txtbuffer);
    }
#endif
    return TRUE;
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
           a->viewport_h == b->viewport_h &&
           a->texWidth == b->texWidth &&
           a->texHeight == b->texHeight;
}

static int SnapshotPixelCompare(const GXFrameSnapshot *current,
                                const GXFrameSnapshot *baseline,
                                int px, int py, int *differs)
{
    uint16_t currentRaw, baselineRaw;

    if (current == NULL || baseline == NULL || differs == NULL)
        return 0;
    *differs = 0;
    if (!ReadSnapshotPixelRaw(current, px, py, &currentRaw) ||
        !ReadSnapshotPixelRaw(baseline, px, py, &baselineRaw))
        return 0;

    *differs = currentRaw != baselineRaw;
    return 1;
}

static int SnapshotBaselineResolutionEligible(
    const GXFrameSnapshot *selected,
    const GXFrameSnapshot *required,
    const GXFrameSnapshot *baseline,
    int samplingMapMatches, int tx, int ty, int pixelCompareSucceeded)
{
    const EfbTileState *base;

    if (baseline == NULL)
        return 0;
    base = &baseline->efbTile[ty & 31][tx & 63];
    return T6FilteredResolutionEligible(
        selected == required, samplingMapMatches,
        base->cover == EFB_TILE_FULL, base->seq,
        g_psxWriteTileEpoch[ty & 31][tx & 63],
        pixelCompareSucceeded);
}

static void InvalidateTileRunRect(void *user, int x0, int y0, int x1, int y1)
{
    (void)user;
#ifdef DISP_DEBUG
    if ((dwActFixes & AUTO_FIX_DINO_CRISIS2) &&
        (g_dc2PauseDiagBarrierSerial & 3u) == 0 &&
        g_dc2PauseDiagInvalidateLogs < 2048)
    {
        g_dc2PauseDiagInvalidateLogs++;
        sprintf(txtbuffer,
                "TRB DC2 INVAL event=%u barrier=%u rect=%d,%d-%d,%d "
                "std=%u/%u wnd=%u/%u\r\n",
                g_dc2PauseDiagEventSerial, g_dc2PauseDiagBarrierSerial,
                x0, y0, x1, y1,
                g_textureStandardCacheHits, g_textureStandardUploads,
                g_textureWindowCacheHits, g_textureWindowUploads);
        writeLogFile(txtbuffer);
    }
#endif
    InvalidateTextureArea(x0, y0, x1 - x0, y1 - y0);
}

static void MergeReadbackToPsxVuwFiltered(
    int x, int y, int w, int h,
    const GXFrameSnapshot *requiredSnapshot,
    const GXFrameSnapshot *baseline)
{
    unsigned char writtenTile[VRAM_READBACK_TILE_Y][VRAM_READBACK_TILE_X];
    union
    {
        unsigned int c0Written
            [VRAM_READBACK_TILE_Y][VRAM_READBACK_TILE_X];
        uint16_t filteredResolved
            [VRAM_READBACK_TILE_Y][VRAM_READBACK_TILE_X];
    } tileCount;
    uint64_t tileSnapSeq[VRAM_READBACK_TILE_Y][VRAM_READBACK_TILE_X];
    unsigned char tileMixed[VRAM_READBACK_TILE_Y][VRAM_READBACK_TILE_X];
    MappingKind mapping;
    uint32_t mapId;
    int baselineSamplingMatches;
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

    PublishCompletedAsyncSnapshot();

#ifdef DISP_DEBUG
    g_lastMergedPixels = 0;
    g_lastMergedChangedPixels = 0;
#if T6_BARRIER_DIAG
    g_lastMergedChangedTiles = 0;
    memset(g_lastMergedChangedTileBitmap, 0,
           sizeof(g_lastMergedChangedTileBitmap));
    g_lastMergedPromotedTiles = 0;
    memset(g_lastMergedPromotedTileBitmap, 0,
           sizeof(g_lastMergedPromotedTileBitmap));
    memset(g_lastMergedTileUsed, 0, sizeof(g_lastMergedTileUsed));
    memset(g_lastMergedTileSeq, 0, sizeof(g_lastMergedTileSeq));
    memset(g_lastMergedTileMapId, 0, sizeof(g_lastMergedTileMapId));
    memset(g_lastMergedTileMixed, 0, sizeof(g_lastMergedTileMixed));
    memset(g_lastMergedTileSourceId, 0,
           sizeof(g_lastMergedTileSourceId));
#endif
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

    baselineSamplingMatches =
        requiredSnapshot != NULL && baseline != NULL &&
        SnapshotsShareSamplingMap(requiredSnapshot, baseline);

    memset(writtenTile, 0, sizeof(writtenTile));
    memset(&tileCount, 0, sizeof(tileCount));
    memset(tileSnapSeq, 0, sizeof(tileSnapSeq));
    memset(tileMixed, 0, sizeof(tileMixed));

    for (yy = 0; yy < h; yy++)
    {
        int py = (y + yy) & 511;
        for (xx = 0; xx < w; xx++)
        {
            int px = (x + xx) & 1023;
            int ttx = (px >> 4) & 63;
            int tty = (py >> 4) & 31;
            uint16_t old = GETLE16(psxVuw + py * 1024 + px);
            GXFrameSnapshot *snap = BestOwningSnapshotForPixel(px, py,
                                                               mapping,
                                                               mapId);
            int resolutionEligible = 0;
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
            if (requiredSnapshot != NULL)
            {
                int differs = 0;
                int compareSucceeded =
                    snap == requiredSnapshot && baselineSamplingMatches &&
                    SnapshotPixelCompare(snap, baseline, px, py, &differs);

                resolutionEligible = SnapshotBaselineResolutionEligible(
                    snap, requiredSnapshot, baseline,
                    baselineSamplingMatches, ttx, tty, compareSucceeded);

                if (!differs)
                {
                    /* A matching snapshot/baseline with no delta is a safely
                     * resolved pixel: retaining psxVuw is intended. */
                    if (resolutionEligible)
                        tileCount.filteredResolved[tty][ttx]++;
#ifdef DISP_DEBUG
                    if (old & 0x7fff)
                        newNonZero[half]++;
                    newHash[half] = (newHash[half] ^ old) * 16777619u;
#endif
                    continue;
                }
            }

            uint16_t color = ReadSnapshotPixelToPSX(snap, px, py);
            uint16_t psx = (color & 0x7FFF) | (old & 0x8000);
            {
                uint64_t snapTileSeq = snap->efbTile[tty][ttx].seq;

                if (!T6C0ColorCommitAllowed(
                        snapTileSeq,
                        g_psxMaterializedColorTileEpoch[tty][ttx]))
                {
                    /* A newer already-materialized color epoch protects the
                     * retained psxVuw pixel from this older snapshot. */
                    if (resolutionEligible)
                        tileCount.filteredResolved[tty][ttx]++;
                    continue;
                }
                if (requiredSnapshot != NULL)
                {
                    if (resolutionEligible)
                        tileCount.filteredResolved[tty][ttx]++;
                }
                else
                {
                    tileCount.c0Written[tty][ttx]++;
                    if (tileCount.c0Written[tty][ttx] == 1)
                        tileSnapSeq[tty][ttx] = snapTileSeq;
                    else if (tileSnapSeq[tty][ttx] != snapTileSeq)
                        tileMixed[tty][ttx] = 1;
                }
#if T6_BARRIER_DIAG
                /* A required snapshot is recorded only after all 256 pixels
                 * have been classified below.  The unfiltered fallback keeps
                 * its existing per-pixel evidence semantics. */
                if (requiredSnapshot == NULL)
                    T6MoveRecordUsedTile(
                        (unsigned char *)g_lastMergedTileUsed,
                        (uint64_t *)g_lastMergedTileSeq,
                        (unsigned int *)g_lastMergedTileMapId,
                        (unsigned char *)g_lastMergedTileMixed,
                        (uint64_t *)g_lastMergedTileSourceId,
                        tty * VRAM_READBACK_TILE_X + ttx,
                        snapTileSeq, mapId, snap->captureOrder);
#endif
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
                int ttx = (px >> 4) & 63;
                int tty = (py >> 4) & 31;
                unsigned int bitIdx =
                    (unsigned int)(tty * VRAM_READBACK_TILE_X + ttx);

                g_lastMergedChangedPixels++;
                if ((old & 0x7FFF) == (psx & 0x7FFF))
                    g_lastMergedMaskOnlyPixels++;
                else
                    g_lastMergedRgbChangedPixels++;
#if T6_BARRIER_DIAG
                if (!(g_lastMergedChangedTileBitmap[bitIdx >> 5] &
                      (1u << (bitIdx & 31))))
                {
                    g_lastMergedChangedTileBitmap[bitIdx >> 5] |=
                        1u << (bitIdx & 31);
                    g_lastMergedChangedTiles++;
                }
#endif
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

#if T6_BARRIER_DIAG
    if (requiredSnapshot != NULL)
    {
        for (ty = 0; ty < VRAM_READBACK_TILE_Y; ty++)
            for (tx = 0; tx < VRAM_READBACK_TILE_X; tx++)
                T6MoveRecordResolvedTileIfFull(
                    (unsigned char *)g_lastMergedTileUsed,
                    (uint64_t *)g_lastMergedTileSeq,
                    (unsigned int *)g_lastMergedTileMapId,
                    (unsigned char *)g_lastMergedTileMixed,
                    (uint64_t *)g_lastMergedTileSourceId,
                    ty * VRAM_READBACK_TILE_X + tx,
                    tileCount.filteredResolved[ty][tx], 0,
                    requiredSnapshot->efbTile[ty][tx].seq,
                    mapId, requiredSnapshot->captureOrder);
    }
#endif

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

    /* A filtered merge promotes a tile after all 256 pixels were safely
     * classified against its required snapshot, including retained no-delta
     * pixels.  Unfiltered C0 retains its full-pixel-write rule.  Promotion is
     * independent from writtenTile, so equal colors do not invalidate cache.
     * This runs after pixel writeback and before cache invalidation. */
    {
        int ty, tx;

        for (ty = 0; ty < VRAM_READBACK_TILE_Y; ty++)
            for (tx = 0; tx < VRAM_READBACK_TILE_X; tx++)
            {
                uint64_t before =
                    g_psxMaterializedColorTileEpoch[ty][tx];
                unsigned int promotionPixels;
                int promotionSingleSource;
                uint64_t promotionSeq;

                if (requiredSnapshot != NULL)
                {
                    promotionPixels = tileCount.filteredResolved[ty][tx];
                    promotionSingleSource = TRUE;
                    promotionSeq =
                        requiredSnapshot->efbTile[ty][tx].seq;
                }
                else
                {
                    promotionPixels = tileCount.c0Written[ty][tx];
                    promotionSingleSource = !tileMixed[ty][tx];
                    promotionSeq = tileSnapSeq[ty][tx];
                }

                g_psxMaterializedColorTileEpoch[ty][tx] =
                    T6FullTileResolutionPromotionEpoch(
                        promotionPixels, promotionSingleSource,
                        promotionSeq, before);
#if T6_BARRIER_DIAG
                if (g_psxMaterializedColorTileEpoch[ty][tx] > before)
                {
                    unsigned int bitIdx =
                        (unsigned int)(ty * VRAM_READBACK_TILE_X + tx);

                    if (!(g_lastMergedPromotedTileBitmap[bitIdx >> 5] &
                          (1u << (bitIdx & 31))))
                    {
                        g_lastMergedPromotedTileBitmap[bitIdx >> 5] |=
                            1u << (bitIdx & 31);
                        g_lastMergedPromotedTiles++;
                    }
                }
#endif
            }
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
    int captureResult;

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

    captureResult = CaptureEfbSnapshotWithTiles(
        slot, &map, tiles,
        mapping == MAPPING_CURRENT ? SNAPSHOT_SOURCE_CURRENT_EFB :
                                     SNAPSHOT_SOURCE_PRESENTED);
    if (captureResult == CAPTURE_QUEUED &&
        CompletePendingAsyncSnapshotForRead())
        captureResult = CAPTURE_COMPLETE;
    return captureResult;
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
    if (g_vramReadBarrierLastReason != T6_REASON_NONE &&
        g_t6ValidationFailureTileX < 0)
    {
        const EfbTileState *base =
            &g_rebuildBaseline.efbTile[ty & 31][tx & 63];

        g_t6ValidationFailureTileX = tx;
        g_t6ValidationFailureTileY = ty;
        g_t6ValidationFailureBaselineSeq =
            in->baselinePresentForSnapshot ? base->seq : 0;
        g_t6ValidationFailureCpuWriteEpoch = in->cpuWriteEpoch;
        g_t6ValidationFailureMaterializedEpoch =
            in->materializedColorEpoch;
        g_t6ValidationFailureSnapshotSeq = in->snapshotSeq;
        g_t6ValidationFailureRequiredSeq = requiredSeq;
        g_t6ValidationFailureSourceId = snap ? snap->captureOrder : 0;
        g_t6ValidationFailureMapId = mapId;
    }
#endif
    return 1;
}

static void WriteMaterializeTile(void *user, VramMaterializePlan *plan,
                                 int tx, int ty, uint64_t requiredSeq)
{
    VramMaterializeSession *session = (VramMaterializeSession *)user;
    T6RGB5A3TileSamplePlan samplePlan;
    MappingKind mapping;
    uint32_t mapId;
    GXFrameSnapshot *snap;
    int baselineDelta;
    int fastPlan;
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
    fastPlan = T6BuildRGB5A3TileSamplePlan(
        &samplePlan, tx, ty,
        snap->vram_x, snap->vram_y, snap->vram_w, snap->vram_h,
        snap->viewport_x, snap->viewport_y,
        snap->viewport_w, snap->viewport_h,
        snap->texWidth, snap->texHeight);
#if T6_BARRIER_DIAG
    if (fastPlan)
        g_vramReadBarrierSampleFastTiles++;
    else
        g_vramReadBarrierSampleFallbackTiles++;
#endif

    for (py = ty << 4; py < (ty << 4) + VRAM_READBACK_TILE_SIZE; py++)
        for (px = tx << 4; px < (tx << 4) + VRAM_READBACK_TILE_SIZE; px++)
        {
            unsigned short currentRaw = 0;
            unsigned short old;
            unsigned short color;
            unsigned short psx;
            int currentRawReady = 0;

            if (fastPlan)
            {
                int lx = px & (VRAM_READBACK_TILE_SIZE - 1);
                int ly = py & (VRAM_READBACK_TILE_SIZE - 1);

                currentRaw = T6ReadRGB5A3TileSample(
                    (const unsigned char *)snap->pixels,
                    &samplePlan, lx, ly);
                currentRawReady = 1;
                if (baselineDelta)
                {
                    unsigned short baselineRaw = T6ReadRGB5A3TileSample(
                        (const unsigned char *)g_rebuildBaseline.pixels,
                        &samplePlan, lx, ly);

                    if (currentRaw == baselineRaw)
                        continue;
                }
            }

            if (!fastPlan && baselineDelta)
            {
                int differs;

                if (!SnapshotPixelCompare(
                        snap, &g_rebuildBaseline, px, py, &differs) ||
                    !differs)
                    continue;
            }

            /* Delay psxVuw access until a baseline delta is proven.  The
             * common no-delta tile now touches only the two snapshot buffers.
             * If plan construction ever rejects an unusual map, retain the
             * original scalar reader instead of weakening correctness. */
            old = GETLE16(psxVuw + (py << 10) + px);
            color = currentRawReady ? GXRGB5A3ToPSX15(currentRaw) :
                                      ReadSnapshotPixelToPSX(snap, px, py);
            psx = T6MergePixelColor(old, color);
            if (psx == old)
                continue;

            PUTLE16(psxVuw + (py << 10) + px, psx);
            T6MaterializePlanMarkChanged(plan, tx, ty);
        }
#if T6_BARRIER_DIAG
    if (!plan->changed[ty & 31][tx & 63])
        g_vramReadBarrierSampleNoDeltaTiles++;
#endif
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
    uint64_t newest;
    int i;

    (void)user;
    if (seq > g_psxMaterializedColorTileEpoch[ty & 31][tx & 63])
        g_psxMaterializedColorTileEpoch[ty & 31][tx & 63] = seq;

    /* Materialization can retire the last potential hazard for this tile.
     * Recompute from every retained source; invalid sources intentionally do
     * not keep the conservative bit alive. */
    newest = g_efbTileState[ty & 31][tx & 63].seq;
    if (g_pendingPresentedReady &&
        g_pendingPresentedTile[ty & 31][tx & 63].seq > newest)
        newest = g_pendingPresentedTile[ty & 31][tx & 63].seq;
    for (i = 0; i < 2; i++)
        if (g_snapshot[i].valid &&
            g_snapshot[i].efbTile[ty & 31][tx & 63].seq > newest)
            newest = g_snapshot[i].efbTile[ty & 31][tx & 63].seq;

    if (newest > PsxVuwColorTileEpoch(
            g_psxWriteTileEpoch[ty & 31][tx & 63],
            g_psxMaterializedColorTileEpoch[ty & 31][tx & 63]))
        g_t6PotentialHazardTile[ty & 31][(tx & 63) >> 5] |=
            1u << (tx & 31);
    else
        g_t6PotentialHazardTile[ty & 31][(tx & 63) >> 5] &=
            ~(1u << (tx & 31));
}

/* Two-phase materialize: prove every hazard tile first, then write, commit
 * epochs, and invalidate only after the whole batch is safe. */
static VramFreshResult MaterializeVramReadCore(
    const VramReadDependency *dep, unsigned int *changedTilesOut)
{
    VramMaterializeSession session;
    T6MaterializeObserve obs;
    int slotIndex;
    int oldestIndex;

    PublishCompletedAsyncSnapshot();

    if (changedTilesOut != NULL)
        *changedTilesOut = 0;

    if (!ReadbackEnabled() || dep == NULL || dep->count == 0 ||
        dep->count > T6_DEP_RECT_CAPACITY)
        return VRAM_FRESH_UNRESOLVED;

#if T6_BARRIER_DIAG
    g_vramReadBarrierLastReason = T6_REASON_NONE;
    g_vramReadBarrierCopyTex = 0;
    g_vramReadBarrierChangedTiles = 0;
    g_vramReadBarrierInvalidateRuns = 0;
    g_vramReadBarrierCaptureUs = 0;
    g_vramReadBarrierWriteUs = 0;
    g_vramReadBarrierInvalUs = 0;
    g_vramReadBarrierPaletteChecks = 0;
    g_vramReadBarrierInvalidatedEntries = 0;
    g_vramReadBarrierSampleFastTiles = 0;
    g_vramReadBarrierSampleFallbackTiles = 0;
    g_vramReadBarrierSampleNoDeltaTiles = 0;
#endif

    /* Most texture reads never overlap color newer than psxVuw.  Avoid the
     * workspace clear plus physical/snapshot ownership scan in that case.
     * An untracked draw has no safe spatial summary and therefore fails
     * closed into the original path. */
    if (!g_untrackedEfbWrite &&
        !T6DependencyIntersectsPotentialTiles(
            dep, g_t6PotentialHazardTile))
    {
#if T6_BARRIER_DIAG
        g_vramReadBarrierFastReject++;
#endif
        return VRAM_FRESH_NO_ACTION;
    }

    /* This dependency really can observe GPU-newer data.  If a frame-end or
     * baseline copy still owns the single staging buffer, complete and publish
     * it now.  Observation happens afterwards so that a just-published
     * snapshot can satisfy the dependency without issuing another copy. */
    if (g_asyncCaptureInFlight &&
        !CompletePendingAsyncSnapshotForRead())
        return VRAM_FRESH_UNRESOLVED;

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
#if T6_BARRIER_DIAG
        {
            uint64_t captureStart = (uint64_t)gettime();
            int captureResult = CaptureEfbForMaterialize(
                &g_snapshot[slotIndex],
                (MappingKind)session.captureMappingKind,
                session.captureMapId,
                session.captureTileX, session.captureTileY);

            g_vramReadBarrierCaptureUs =
                (unsigned int)ticks_to_microsecs(
                    (uint64_t)gettime() - captureStart);
            if (captureResult == CAPTURE_REJECTED)
                return VRAM_FRESH_UNRESOLVED;
            T6CopyTexCapture(&g_vramReadBarrierCopyTex,
                             &g_vramReadBarrierCopyTexTotal);
            if (captureResult != CAPTURE_COMPLETE)
                return VRAM_FRESH_UNRESOLVED;
        }
#else
        {
            int captureResult = CaptureEfbForMaterialize(
                &g_snapshot[slotIndex],
                (MappingKind)session.captureMappingKind,
                session.captureMapId,
                session.captureTileX, session.captureTileY);
            if (captureResult != CAPTURE_COMPLETE)
                return VRAM_FRESH_UNRESOLVED;
        }
#endif
#if T6_BARRIER_DIAG
        g_vramReadBarrierLastReason = T6_REASON_NONE;
#endif
    }

    if (session.plan.hazardCount == 0)
        return VRAM_FRESH_NO_ACTION;

    /* Re-prove the original hazard batch; capture may have replaced a slot,
     * so a missing snapshot now means the whole barrier is unresolved. */
    if (!T6MaterializeSessionValidate(&session,
                                      ValidateMaterializeTile, &session))
        return VRAM_FRESH_UNRESOLVED;

#if T6_BARRIER_DIAG
    {
        uint64_t writeStart = (uint64_t)gettime();

        T6MaterializeSessionWrite(&session, WriteMaterializeTile, &session);
        T6MaterializeSessionCommitEpoch(&session,
                                        MaterializeSnapshotSeq,
                                        ApplyMaterializeEpoch, &session);
        g_vramReadBarrierWriteUs =
            (unsigned int)ticks_to_microsecs(
                (uint64_t)gettime() - writeStart);
    }
    g_vramReadBarrierChangedTiles =
        (unsigned int)session.plan.changedCount;
#else
    T6MaterializeSessionWrite(&session, WriteMaterializeTile, &session);
    T6MaterializeSessionCommitEpoch(&session,
                                    MaterializeSnapshotSeq,
                                    ApplyMaterializeEpoch, &session);
#endif

    if (session.plan.changedCount > 0)
    {
#if T6_BARRIER_DIAG
        uint64_t invalStart = (uint64_t)gettime();
        unsigned int paletteBefore = g_texturePaletteEntryChecks;
        unsigned int invalidatedBefore = g_textureTotalInvalidatedEntries;
#endif
        int runs = T6MaterializeSessionInvalidateChanged(
            &session, InvalidateTileRunRect, NULL);
#if T6_BARRIER_DIAG
        g_vramReadBarrierInvalidateRuns = (unsigned int)runs;
        g_vramReadBarrierInvalUs =
            (unsigned int)ticks_to_microsecs(
                (uint64_t)gettime() - invalStart);
        g_vramReadBarrierPaletteChecks =
            g_texturePaletteEntryChecks - paletteBefore;
        g_vramReadBarrierInvalidatedEntries =
            g_textureTotalInvalidatedEntries - invalidatedBefore;
#endif
        (void)runs;
    }

    if (changedTilesOut != NULL)
        *changedTilesOut = (unsigned int)session.plan.changedCount;
    return VRAM_FRESH_MATERIALIZED;
}

static VramFreshResult MaterializeVramRead(
    const VramReadDependency *dep, unsigned int *changedTilesOut)
{
    VramFreshResult result;

    if (T6ReadFreshEntry(
            ReadbackEnabled(),
            dep != NULL && dep->count > 0 &&
                dep->count <= T6_DEP_RECT_CAPACITY,
            1, 1, changedTilesOut) != VRAM_FRESH_NO_ACTION)
        return VRAM_FRESH_UNRESOLVED;

    if (!T6WorkspaceBegin(&g_t6WorkspaceDepth))
    {
        (void)T6ReadFreshEntry(1, 1, 1, 0, changedTilesOut);
        return VRAM_FRESH_UNRESOLVED;
    }

    result = MaterializeVramReadCore(dep, changedTilesOut);
    T6WorkspaceEnd(&g_t6WorkspaceDepth);
    return result;
}

#ifdef DISP_DEBUG
static unsigned int DC2PauseDiagRectHash(const VramRect *rect)
{
    unsigned int hash = 2166136261u;
    int xs[3], ys[3];
    int ix, iy;

    if (rect == NULL || rect->x1 <= rect->x0 || rect->y1 <= rect->y0)
        return 0;

    xs[0] = rect->x0;
    xs[1] = (rect->x0 + rect->x1 - 1) / 2;
    xs[2] = rect->x1 - 1;
    ys[0] = rect->y0;
    ys[1] = (rect->y0 + rect->y1 - 1) / 2;
    ys[2] = rect->y1 - 1;

    for (iy = 0; iy < 3; iy++)
        for (ix = 0; ix < 3; ix++)
        {
            unsigned short pixel = GETLE16(
                psxVuw + ((ys[iy] & 511) << 10) + (xs[ix] & 1023));
            hash = (hash ^ pixel) * 16777619u;
        }
    return hash;
}

static void DC2PauseDiagLogBarrier(
    const VramReadDependency *dependency,
    VramFreshResult result, unsigned int changedTiles)
{
    unsigned int i, rectCount;
    int targetKind;
    int interesting;

    if (!(dwActFixes & AUTO_FIX_DINO_CRISIS2) || dependency == NULL)
        return;

    g_dc2PauseDiagLastResult = (int)result;
    g_dc2PauseDiagLastChanged = changedTiles;

    interesting =
        (result == VRAM_FRESH_MATERIALIZED && changedTiles != 0 &&
         (g_dc2PauseDiagBarrierSerial & 3u) == 0) ||
        (result == VRAM_FRESH_UNRESOLVED && g_asyncCaptureInFlight &&
         (g_dc2PauseDiagBarrierSerial & 31u) == 0) ||
        ((g_dc2PauseDiagBarrierSerial & 511u) == 0);
    if (!interesting || g_dc2PauseDiagEventLogs >= 1024)
        return;

    g_dc2PauseDiagEventSerial++;
    g_dc2PauseDiagEventLogs++;
    g_dc2PauseDiagDrawBudget = 8;

    targetKind = g_asyncTarget == &g_snapshot[0] ? 0 :
                 (g_asyncTarget == &g_snapshot[1] ? 1 :
                  (g_asyncTarget == &g_rebuildBaseline ? 2 :
#if defined(VRAM_CONTENT_DIAG)
                   (g_asyncTarget == &g_diagBaseline ? 3 : 4)));
#else
                   4));
#endif

    sprintf(txtbuffer,
            "TRB DC2 MAT event=%u barrier=%u result=%d changed=%u "
            "kind=%d rects=%u async=%d/%d target=%d token=%u "
            "aOrder=%u aSrc=%d aMap=%u aSeq=%u "
            "active=%u/%u dirty=%d pending=%d/%u "
            "snap0=%d/%u/%u/%u snap1=%d/%u/%u/%u "
            "base=%d/%u/%u\r\n",
            g_dc2PauseDiagEventSerial, g_dc2PauseDiagBarrierSerial,
            (int)result, changedTiles, (int)dependency->kind,
            dependency->count,
            g_asyncCaptureInFlight, g_asyncCaptureReady,
            targetKind, (unsigned int)g_asyncCaptureToken,
            (unsigned int)g_asyncSnapshot.captureOrder,
            (int)g_asyncSnapshot.source, g_asyncSnapshot.map_id,
            (unsigned int)g_asyncSnapshot.capturedContentSeq,
            g_activeMap.map_id,
            (unsigned int)g_activeMap.contentWriteSeq,
            g_activeMap.content_dirty,
            g_pendingPresentedReady,
            (unsigned int)g_pendingPresentedCaptureOrder,
            g_snapshot[0].valid, g_snapshot[0].map_id,
            (unsigned int)g_snapshot[0].capturedContentSeq,
            (unsigned int)g_snapshot[0].captureOrder,
            g_snapshot[1].valid, g_snapshot[1].map_id,
            (unsigned int)g_snapshot[1].capturedContentSeq,
            (unsigned int)g_snapshot[1].captureOrder,
            g_rebuildBaseline.valid, g_rebuildBaseline.map_id,
            (unsigned int)g_rebuildBaseline.capturedContentSeq);
    writeLogFile(txtbuffer);

    rectCount = dependency->count < 4 ? dependency->count : 4;
    for (i = 0; i < rectCount; i++)
    {
        const VramRect *rect = &dependency->rect[i];
        int tx = (rect->x0 >> 4) & 63;
        int ty = (rect->y0 >> 4) & 31;

        sprintf(txtbuffer,
                "TRB DC2 RECT event=%u i=%u rect=%d,%d-%d,%d "
                "hash=%08X tile=%d,%d epoch=%u/%u efb=%u "
                "hazard=%u\r\n",
                g_dc2PauseDiagEventSerial, i,
                rect->x0, rect->y0, rect->x1, rect->y1,
                DC2PauseDiagRectHash(rect), tx, ty,
                (unsigned int)g_psxWriteTileEpoch[ty][tx],
                (unsigned int)g_psxMaterializedColorTileEpoch[ty][tx],
                (unsigned int)g_efbTileState[ty][tx].seq,
                (g_t6PotentialHazardTile[ty][tx >> 5] >> (tx & 31)) & 1u);
        writeLogFile(txtbuffer);
    }
}
#endif

/* Texture-path barrier entry with a release-usable changed-tile result.  The
 * counter lets callers distinguish a same-color materialize from one that
 * really changed pixels.  Returns UNRESOLVED on reentrancy/disabled state so
 * a nested lookup can never paper over stale VRAM with success. */
static VramFreshResult EnsureVramReadFreshEx(
    const VramReadDependency *dependency, unsigned int *changedTilesOut)
{
    VramFreshResult result;
    unsigned int localChangedTiles = 0;
    unsigned int *effectiveChangedTiles = changedTilesOut;
#if T6_BARRIER_DIAG
    uint64_t ticksStart = (uint64_t)gettime();
    uint64_t ticksEnd;
    unsigned int callUs;
#endif

#ifdef DISP_DEBUG
    if ((dwActFixes & AUTO_FIX_DINO_CRISIS2) &&
        effectiveChangedTiles == NULL)
        effectiveChangedTiles = &localChangedTiles;
    if (dwActFixes & AUTO_FIX_DINO_CRISIS2)
        g_dc2PauseDiagBarrierSerial++;
#else
    (void)localChangedTiles;
#endif

    if (T6ReadFreshEntry(
            ReadbackMaterializationEnabled(),
            dependency != NULL && dependency->count > 0 &&
                dependency->count <= T6_DEP_RECT_CAPACITY,
            1, 1, changedTilesOut) != VRAM_FRESH_NO_ACTION)
        return VRAM_FRESH_UNRESOLVED;

#if T6_BARRIER_DIAG
    g_vramReadBarrierCalls++;
#endif

    if (!T6WorkspaceBegin(&g_vramReadBarrierDepth))
    {
        (void)T6ReadFreshEntry(1, 1, 0, 1, changedTilesOut);
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

    result = MaterializeVramRead(dependency, effectiveChangedTiles);
    T6WorkspaceEnd(&g_vramReadBarrierDepth);

#ifdef DISP_DEBUG
    DC2PauseDiagLogBarrier(
        dependency, result,
        effectiveChangedTiles != NULL ? *effectiveChangedTiles : 0);
#endif

#if T6_BARRIER_DIAG
    ticksEnd = (uint64_t)gettime();
    callUs = (unsigned int)ticks_to_microsecs(ticksEnd - ticksStart);
    g_vramReadBarrierUsTotal += callUs;
    g_vramReadBarrierCaptureUsTotal += g_vramReadBarrierCaptureUs;
    g_vramReadBarrierWriteUsTotal += g_vramReadBarrierWriteUs;
    g_vramReadBarrierInvalUsTotal += g_vramReadBarrierInvalUs;
    g_vramReadBarrierChangedTilesTotal += g_vramReadBarrierChangedTiles;
    g_vramReadBarrierInvalidateRunsTotal +=
        g_vramReadBarrierInvalidateRuns;
    g_vramReadBarrierPaletteChecksTotal +=
        g_vramReadBarrierPaletteChecks;
    g_vramReadBarrierInvalidatedEntriesTotal +=
        g_vramReadBarrierInvalidatedEntries;
    g_vramReadBarrierSampleFastTilesTotal +=
        g_vramReadBarrierSampleFastTiles;
    g_vramReadBarrierSampleFallbackTilesTotal +=
        g_vramReadBarrierSampleFallbackTiles;
    g_vramReadBarrierSampleNoDeltaTilesTotal +=
        g_vramReadBarrierSampleNoDeltaTiles;
    if (callUs > g_vramReadBarrierUsMax)
        g_vramReadBarrierUsMax = callUs;
    if (g_vramReadBarrierCaptureUs > g_vramReadBarrierCaptureUsMax)
        g_vramReadBarrierCaptureUsMax = g_vramReadBarrierCaptureUs;
    if (callUs >= 1000)
        g_vramReadBarrierSlowCalls++;

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
                    "TRB RES result=unresolved reason=%d calls=%u "
                    "copyTex=%u copyTexTotal=%u changed=%u invalRuns=%u "
                    "us=%u\r\n",
                    reason, g_vramReadBarrierCalls,
                    g_vramReadBarrierCopyTex,
                    g_vramReadBarrierCopyTexTotal,
                    g_vramReadBarrierChangedTiles,
                    g_vramReadBarrierInvalidateRuns,
                    callUs);
            writeLogFile(txtbuffer);
        }
    }
    else if (g_vramReadBarrierSuccessLogs < 32)
    {
        g_vramReadBarrierSuccessLogs++;
        sprintf(txtbuffer,
                "TRB RES result=%d calls=%u noAction=%u materialized=%u "
                "copyTex=%u copyTexTotal=%u changed=%u invalRuns=%u "
                "us=%u capUs=%u writeUs=%u invalUs=%u "
                "sampleFast=%u sampleFallback=%u sampleNoDelta=%u "
                "palChecks=%u "
                "invalidated=%u\r\n",
                (int)result, g_vramReadBarrierCalls,
                g_vramReadBarrierNoAction,
                g_vramReadBarrierMaterialized,
                g_vramReadBarrierCopyTex,
                g_vramReadBarrierCopyTexTotal,
                g_vramReadBarrierChangedTiles,
                g_vramReadBarrierInvalidateRuns,
                callUs, g_vramReadBarrierCaptureUs,
                g_vramReadBarrierWriteUs, g_vramReadBarrierInvalUs,
                g_vramReadBarrierSampleFastTiles,
                g_vramReadBarrierSampleFallbackTiles,
                g_vramReadBarrierSampleNoDeltaTiles,
                g_vramReadBarrierPaletteChecks,
                g_vramReadBarrierInvalidatedEntries);
        writeLogFile(txtbuffer);
    }

    /* Bounded lifetime telemetry: 1,2,4,8... calls only.  This remains useful
     * after the per-call log limits are exhausted without turning logging
     * itself into the performance problem under investigation. */
    if ((g_vramReadBarrierCalls & (g_vramReadBarrierCalls - 1u)) == 0)
    {
        sprintf(txtbuffer,
                "TRB PERF calls=%u noAction=%u fastReject=%u mat=%u unr=%u copy=%u "
                "totalUs=%llu avgUs=%llu maxUs=%u slow=%u "
                "capUs=%llu capMax=%u writeUs=%llu invalUs=%llu "
                "sampleFast=%llu sampleFallback=%llu sampleNoDelta=%llu "
                "changed=%llu runs=%llu palChecks=%llu invalidated=%llu "
                "stdHit=%u stdUp=%u wndHit=%u wndUp=%u\r\n",
                g_vramReadBarrierCalls, g_vramReadBarrierNoAction,
                g_vramReadBarrierFastReject,
                g_vramReadBarrierMaterialized, g_vramReadBarrierUnresolved,
                g_vramReadBarrierCopyTexTotal,
                (unsigned long long)g_vramReadBarrierUsTotal,
                (unsigned long long)(g_vramReadBarrierUsTotal /
                                     g_vramReadBarrierCalls),
                g_vramReadBarrierUsMax, g_vramReadBarrierSlowCalls,
                (unsigned long long)g_vramReadBarrierCaptureUsTotal,
                g_vramReadBarrierCaptureUsMax,
                (unsigned long long)g_vramReadBarrierWriteUsTotal,
                (unsigned long long)g_vramReadBarrierInvalUsTotal,
                (unsigned long long)g_vramReadBarrierSampleFastTilesTotal,
                (unsigned long long)g_vramReadBarrierSampleFallbackTilesTotal,
                (unsigned long long)g_vramReadBarrierSampleNoDeltaTilesTotal,
                (unsigned long long)g_vramReadBarrierChangedTilesTotal,
                (unsigned long long)g_vramReadBarrierInvalidateRunsTotal,
                (unsigned long long)g_vramReadBarrierPaletteChecksTotal,
                (unsigned long long)g_vramReadBarrierInvalidatedEntriesTotal,
                g_textureStandardCacheHits, g_textureStandardUploads,
                g_textureWindowCacheHits, g_textureWindowUploads);
        writeLogFile(txtbuffer);
    }
#endif

    return result;
}

static VramFreshResult EnsureVramReadFresh(
    const VramReadDependency *dependency)
{
    return EnsureVramReadFreshEx(dependency, NULL);
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

    MaterializeVramRead(&dep, NULL);
}

/* T6-E-2 建立、T6-E-3 唯一保留的通用 MoveImage source freshness wrapper。
 * 所有 GP0(80h) 在计算/持有 CPU source pointer或执行 CPU copy 前调用。契约:
 * 1. iGPUHeight != 512、非法 rect、dependency overflow、readback disabled
 *    或 reentrant 时返回 VRAM_FRESH_UNRESOLVED,不得留下截断 dependency
 *    (BuildMoveSourceDependency 失败已把 dep.count 清 0),也不得写任何状态;
 * 2. 合法 512-line source 用 BuildMoveSourceDependency() 再调
 *    EnsureVramReadFreshEx()(真实 capture/merge/epoch-commit/invalidate);
 * 3. 返回原始三态结果与 changed tile 数,不把 UNRESOLVED 改写成 NO_ACTION;
 * 4. 不在 wrapper 内执行 CPU copy或 destination ownership。 */
static VramFreshResult EnsureVramMoveSourceFresh(
    int x, int y, int w, int h, unsigned int *changedTilesOut)
{
    VramReadDependency dep;

    T6ResetChangedTilesOutput(changedTilesOut);

    if (!T6MoveSourceFreshnessPrecheck(
            x, y, w, h, iGPUHeight, ReadbackMaterializationEnabled(),
            g_t6WorkspaceDepth, g_vramReadBarrierDepth))
        return VRAM_FRESH_UNRESOLVED;

    memset(&dep, 0, sizeof(dep));
    if (!BuildMoveSourceDependency(x, y, w, h, 1024, 512, &dep))
        return VRAM_FRESH_UNRESOLVED;

    return EnsureVramReadFreshEx(&dep, changedTilesOut);
}

#if T6_BARRIER_DIAG
/* Bounded before/after content evidence for the generic MoveImage takeover. */
static unsigned int T6VramRectHashWrapped(int x, int y, int w, int h)
{
    VramRect pieces[4];
    int n, i, py, px;
    unsigned int hash = 2166136261u;

    n = SplitWrappedVramRect(x, y, w, h, 1024, iGPUHeight, pieces);
    for (i = 0; i < n; i++)
        for (py = pieces[i].y0; py < pieces[i].y1; py++)
            for (px = pieces[i].x0; px < pieces[i].x1; px++)
            {
                unsigned int v = GETLE16(psxVuw + (py << 10) + px);
                hash = (hash ^ v) * 16777619u;
            }
    return hash;
}
#endif /* T6_BARRIER_DIAG */

#if T6_BARRIER_DIAG
/* Read-only MoveImage source freshness decision.  It runs the same dependency
 * observation and pure validate decision as the texture barrier but never
 * captures, writes psxVuw, commits epochs or invalidates the cache. */
static int T6VramMoveCapturePlanForSession(
    const VramMaterializeSession *session, int *outSlot)
{
    int oldestIndex, slotIndex;
    GXFrameSnapshot *oldest = OldestSnapshotSlot();

    if (session == NULL || !session->captureRequired ||
        session->captureConflict)
        return 0;
    oldestIndex = oldest == &g_snapshot[0] ? 0 : 1;
    if (!T6MaterializeSessionSelectCaptureSlot(
            session, oldestIndex, &slotIndex))
        return 0;

    if (session->captureMappingKind == MAPPING_CURRENT)
    {
        if (!CanCaptureActiveEfb())
            return 0;
    }
    else if (session->captureMappingKind == MAPPING_PREVIOUS)
    {
        if (!g_pendingPresentedReady ||
            session->captureMapId != g_prevMapId ||
            !RectIntersectsTile(g_pendingPresentedMap.vram_x0,
                                g_pendingPresentedMap.vram_y0,
                                g_pendingPresentedMap.vram_x1,
                                g_pendingPresentedMap.vram_y1,
                                session->captureTileX,
                                session->captureTileY) ||
            (STATUSREG & GPUSTATUS_RGB24) || PSXDisplay.RGB24 ||
            g_efbContaminated || g_mixedMappingSeen ||
            g_untrackedEfbWrite)
            return 0;
    }
    else
        return 0;

    if (outSlot != NULL)
        *outSlot = slotIndex;
    return 1;
}

static int T6VramMoveCaptureSlotBufferReady(int slotIndex)
{
    GXFrameSnapshot *slot;
    int copyW;
    int copyH;
    int requiredSize;

    if (slotIndex < 0 || slotIndex >= 2)
        return 0;
    slot = &g_snapshot[slotIndex];
    copyW = (iResX + 15) & ~15;
    copyH = (iResY + 1) & ~1;
    requiredSize = GX_GetTexBufferSize(copyW, copyH,
                                       GX_TF_RGB5A3, 0, GX_FALSE);
    return T6MoveCaptureBufferReady(
        slot->pixels != NULL, slot->size, requiredSize);
}

/* Build metadata for the snapshot that CaptureEfbForMaterialize() would
 * create, without copying the EFB or touching a real snapshot slot.  Only
 * one tile entry is populated at a time by the post-capture validator below,
 * keeping this DIAG shadow free of a per-call 32 KiB clear/copy. */
static int T6PreparePlannedCaptureSnapshot(
    const VramMaterializeSession *session, GXFrameSnapshot *predicted,
    EfbTileState (**sourceTiles)[VRAM_READBACK_TILE_X])
{
    const GXDisplayMap *map;

    if (session == NULL || predicted == NULL || sourceTiles == NULL ||
        !session->captureRequired)
        return 0;
    if (session->captureMappingKind == MAPPING_CURRENT)
    {
        map = &g_activeMap;
        *sourceTiles = g_efbTileState;
        predicted->source = SNAPSHOT_SOURCE_CURRENT_EFB;
    }
    else if (session->captureMappingKind == MAPPING_PREVIOUS &&
             g_pendingPresentedReady)
    {
        map = &g_pendingPresentedMap;
        *sourceTiles = g_pendingPresentedTile;
        predicted->source = SNAPSHOT_SOURCE_PRESENTED;
    }
    else
        return 0;

    predicted->valid = 1;
    predicted->vram_x = map->vram_x0;
    predicted->vram_y = map->vram_y0;
    predicted->vram_w = map->vram_x1 - map->vram_x0;
    predicted->vram_h = map->vram_y1 - map->vram_y0;
    predicted->viewport_x = map->viewport_x0;
    predicted->viewport_y = map->viewport_y0;
    predicted->viewport_w = map->viewport_x1 - map->viewport_x0;
    predicted->viewport_h = map->viewport_y1 - map->viewport_y0;
    predicted->ratio = map->ratio;
    predicted->map_id = session->captureMapId;
    predicted->exactRectValid = 0;
    predicted->captureOrder = g_captureOrder + 1;
    return 1;
}

/* Select exactly as BestOwningSnapshotForTile() will after the planned slot
 * has been replaced.  The synthetic candidate is metadata-only and never
 * becomes visible to production. */
static const GXFrameSnapshot *T6BestOwningSnapshotAfterPlannedCapture(
    const GXFrameSnapshot *predicted, int replacedSlot,
    int tx, int ty, MappingKind mapping, uint32_t mapId)
{
    T6SnapshotCandidate cands[3];
    const GXFrameSnapshot *refs[3];
    int count = 0;
    int selected;
    int i;

    memset(cands, 0, sizeof(cands));
    for (i = 0; i < 2; i++)
    {
        GXFrameSnapshot *snap = &g_snapshot[i];

        if (i == replacedSlot)
            continue;
        refs[count] = snap;
        if (snap->valid && snap->map_id == mapId &&
            SnapshotCoversTile(snap, tx, ty) &&
            SnapshotExactRectCoversTile(snap, tx, ty) &&
            SnapshotOwnsColorTileForBarrier(snap, tx, ty))
        {
            cands[count].seq = snap->efbTile[ty][tx].seq;
            cands[count].sourcePriority =
                SnapshotSourcePriority(snap, mapping);
            cands[count].captureOrder = snap->captureOrder;
            cands[count].qualified = 1;
        }
        count++;
    }

    refs[count] = predicted;
    if (predicted->valid &&
        T6MoveCaptureCandidateMatchesMap(predicted->map_id, mapId) &&
        SnapshotCoversTile(predicted, tx, ty) &&
        SnapshotExactRectCoversTile(predicted, tx, ty) &&
        SnapshotOwnsColorTileForBarrier(predicted, tx, ty))
    {
        cands[count].seq = predicted->efbTile[ty][tx].seq;
        cands[count].sourcePriority =
            SnapshotSourcePriority(predicted, mapping);
        cands[count].captureOrder = predicted->captureOrder;
        cands[count].qualified = 1;
    }
    count++;

    selected = T6SelectBestSnapshotTile(cands, count, NULL);
    return selected > 0 ? refs[selected - 1] : NULL;
}

/* Read-only simulation of the real post-capture validation.  It predicts
 * the replacement slot and captured metadata, then re-proves every original
 * hazard tile.  This handles FULL coverage, required sequence, per-tile map
 * identity and rebuild-baseline freshness; a capture-map baseline is never
 * applied to a retained snapshot from another map. */
static int T6VramMoveCapturePostValidateForSession(
    const VramMaterializeSession *session, int planSlot,
    VramMoveShadowDecision *out)
{
    static GXFrameSnapshot predicted;
    EfbTileState (*sourceTiles)[VRAM_READBACK_TILE_X] = NULL;
    int i;

    if (!T6PreparePlannedCaptureSnapshot(
            session, &predicted, &sourceTiles))
        return 0;

    for (i = 0; i < session->touchedCount; i++)
    {
        int idx = session->touched[i];
        int tx = idx % T6_VRAM_TILE_X;
        int ty = idx / T6_VRAM_TILE_X;
        uint64_t requiredSeq;
        uint32_t mapId;
        MappingKind mapping;
        T6PhysicalTileState phys;
        const GXFrameSnapshot *snap;
        const EfbTileState *efb;
        VramTileFreshnessInput in;
        int reason = T6_REASON_NONE;

        if (!session->plan.hazard[ty][tx])
            continue;
        requiredSeq =
            T6MaterializeSessionRequiredSeq(session, tx, ty);
        mapId = T6MaterializeSessionRequiredMapId(session, tx, ty);
        mapping = ClassifyTileMapping(tx, ty);

        /* Capture copies this tile metadata even when the tile belongs to a
         * different retained map; map qualification below prevents it from
         * becoming a candidate for that other map. */
        predicted.efbTile[ty][tx] = sourceTiles[ty][tx];
        snap = T6BestOwningSnapshotAfterPlannedCapture(
            &predicted, planSlot, tx, ty, mapping, mapId);

        memset(&phys, 0, sizeof(phys));
        if (mapping != MAPPING_UNKNOWN)
        {
            TilePhysicalEfbState(tx, ty, mapping, mapId, &phys);
            if (!TilePhysicalOwnsTargetMap(&phys, mapping, mapId))
                memset(&phys, 0, sizeof(phys));
        }

        memset(&in, 0, sizeof(in));
        in.efbSeq = requiredSeq;
        in.cpuWriteEpoch = g_psxWriteTileEpoch[ty][tx];
        in.materializedColorEpoch =
            g_psxMaterializedColorTileEpoch[ty][tx];
        in.snapshotSeq = snap ? snap->efbTile[ty][tx].seq : 0;
        in.rgb24 = (STATUSREG & GPUSTATUS_RGB24) || PSXDisplay.RGB24 ||
                   (phys.map != NULL && phys.map->rgb24);
        in.contaminated = g_efbContaminated;
        in.mixedMapping = g_mixedMappingSeen;
        in.untrackedEfb = g_untrackedEfbWrite ||
                          mapping == MAPPING_UNKNOWN || mapId == 0;
        in.hasValidSnapshot = snap != NULL &&
                              in.snapshotSeq >= requiredSeq;
        in.baselinePresentForSnapshot =
            snap != NULL && RebuildBaselineMatchesSnapshot(snap);
        in.baselineUsable =
            snap != NULL && RebuildBaselineUsableForTile(snap, tx, ty);
        efb = snap ? &snap->efbTile[ty][tx] : NULL;
        in.efbCoverFull =
            efb != NULL && efb->cover == EFB_TILE_FULL &&
            SnapshotExactRectCoversTile(snap, tx, ty);

        if (in.rgb24)
            reason = T6_REASON_RGB24;
        else if (in.contaminated || in.mixedMapping || in.untrackedEfb)
            reason = mapping == MAPPING_UNKNOWN || mapId == 0 ?
                     T6_REASON_UNKNOWN_MAP : T6_REASON_HAZARD;
        else if (!in.efbCoverFull)
            reason = T6_REASON_PARTIAL;
        else if (!in.hasValidSnapshot)
            reason = T6_REASON_CAPTURE_FAIL;
        else if (in.baselinePresentForSnapshot && !in.baselineUsable)
            reason = T6_REASON_BASELINE_STALE;

        if (EvaluateVramMaterializeTile(&in) != VRAM_FRESH_MATERIALIZED)
        {
            const EfbTileState *base =
                &g_rebuildBaseline.efbTile[ty][tx];

            if (out != NULL)
            {
                out->unresolvedReason = reason != T6_REASON_NONE ?
                                        reason : T6_REASON_CAPTURE_FAIL;
                out->unresolvedTileX = tx;
                out->unresolvedTileY = ty;
                out->unresolvedBaselineSeq =
                    in.baselinePresentForSnapshot ? base->seq : 0;
                out->unresolvedCpuWriteEpoch = in.cpuWriteEpoch;
                out->unresolvedMaterializedEpoch =
                    in.materializedColorEpoch;
                out->unresolvedSnapshotSeq = in.snapshotSeq;
                out->unresolvedRequiredSeq = requiredSeq;
                out->unresolvedSourceId = snap ? snap->captureOrder : 0;
                out->unresolvedMapId = mapId;
            }
            return 0;
        }
    }
    return 1;
}

static void T6VramMoveShadowDecisionForRect(
    int x, int y, int w, int h, VramMoveShadowDecision *out)
{
    VramReadDependency dep;
    VramMaterializeSession session;
    T6MaterializeObserve obs;
    int i;

    T6VramMoveShadowDecisionInit(out);
    out->unresolvedReason = T6_REASON_NONE;

    if (!ReadbackMaterializationEnabled() || w <= 0 || h <= 0)
    {
        out->unresolvedReason = T6_REASON_CAPTURE_FAIL;
        return;
    }

    /* 1024-line VRAM is outside the 32x64 readback tile model; fail closed
     * instead of reporting a decision for an unenumerable source. */
    if (iGPUHeight != 512)
    {
        out->unresolvedReason = T6_REASON_PARTIAL;
        return;
    }

    memset(&dep, 0, sizeof(dep));
    if (!BuildMoveSourceDependency(x, y, w, h, 1024, iGPUHeight, &dep))
    {
        out->unresolvedReason = T6_REASON_OVERFLOW;
        return;
    }

    if (!T6WorkspaceBegin(&g_t6WorkspaceDepth))
    {
        out->unresolvedReason = T6_REASON_REENTRANT;
        return;
    }
    T6MaterializeSessionInit(&session, &g_t6Workspace);
    memset(&obs, 0, sizeof(obs));
    obs.session = &session;
    ForEachVramReadDependencyTile(&dep, 1024, 512,
                                  ObserveMaterializeTile, &obs);
    T6WorkspaceEnd(&g_t6WorkspaceDepth);

    out->hazardTiles = session.plan.hazardCount;
    out->captureRequired = session.captureRequired;
    out->sourceHash = T6VramRectHashWrapped(x, y, w, h);
    for (i = 0; i < session.touchedCount; i++)
    {
        int idx = session.touched[i];
        int ttx = idx % T6_VRAM_TILE_X;
        int tty = idx / T6_VRAM_TILE_X;

        if (!session.plan.hazard[tty][ttx])
            continue;
        out->requiredTile[tty][ttx] = 1;
        out->requiredTileSeq[tty][ttx] =
            T6MaterializeSessionRequiredSeq(&session, ttx, tty);
        out->requiredTileMapId[tty][ttx] =
            T6MaterializeSessionRequiredMapId(&session, ttx, tty);
        out->requiredTiles++;
        if (out->requiredSeq == 0)
            out->requiredMapId =
                T6MaterializeSessionRequiredMapId(&session, ttx, tty);
        if (T6MaterializeSessionRequiredSeq(&session, ttx, tty) >
            out->requiredSeq)
            out->requiredSeq =
                T6MaterializeSessionRequiredSeq(&session, ttx, tty);
    }
    if (session.overflow)
    {
        out->unresolvedReason = T6_REASON_OVERFLOW;
        return;
    }
    if (session.captureConflict)
    {
        out->unresolvedReason = T6_REASON_HAZARD;
        return;
    }
    if (session.plan.hazardCount == 0)
    {
        out->result = VRAM_FRESH_NO_ACTION;
        out->unresolvedReason = T6_REASON_NONE;
        return;
    }

    /* Capture eligibility, mirroring MaterializeVramReadCore(). */
    if (session.captureRequired)
    {
        T6CaptureDiagInput diag;
        int planSlot = -1;

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
        out->unresolvedReason = T6ClassifyCaptureFailure(&diag);
        if (out->unresolvedReason != T6_REASON_CAPTURE_FAIL)
            return;
        if (!T6VramMoveCapturePlanForSession(&session, &planSlot))
        {
            out->unresolvedReason = T6_REASON_CAPTURE_FAIL;
            return;
        }
        out->wouldCapture = 1;
        out->capturePlanSlot = planSlot;
        out->captureBufferReady =
            T6VramMoveCaptureSlotBufferReady(planSlot);
        if (!out->captureBufferReady)
        {
            out->unresolvedReason = T6_REASON_CAPTURE_FAIL;
            return;
        }
        if (!T6VramMoveCapturePostValidateForSession(
                &session, planSlot, out))
            return;
        out->unresolvedReason = T6_REASON_NONE;
        out->result = VRAM_FRESH_MATERIALIZED;
        return;
    }

    /* No capture needed: pure validate decision on the current snapshot. */
#if T6_BARRIER_DIAG
    g_t6ValidationFailureTileX = -1;
    g_t6ValidationFailureTileY = -1;
    g_t6ValidationFailureBaselineSeq = 0;
    g_t6ValidationFailureCpuWriteEpoch = 0;
    g_t6ValidationFailureMaterializedEpoch = 0;
    g_t6ValidationFailureSnapshotSeq = 0;
    g_t6ValidationFailureRequiredSeq = 0;
    g_t6ValidationFailureSourceId = 0;
    g_t6ValidationFailureMapId = 0;
#endif
    if (!T6MaterializeSessionValidate(&session,
                                      ValidateMaterializeTile, &session))
    {
        out->unresolvedReason =
            g_vramReadBarrierLastReason != T6_REASON_NONE ?
                g_vramReadBarrierLastReason : T6_REASON_CAPTURE_FAIL;
#if T6_BARRIER_DIAG
        out->unresolvedTileX = g_t6ValidationFailureTileX;
        out->unresolvedTileY = g_t6ValidationFailureTileY;
        out->unresolvedBaselineSeq =
            g_t6ValidationFailureBaselineSeq;
        out->unresolvedCpuWriteEpoch =
            g_t6ValidationFailureCpuWriteEpoch;
        out->unresolvedMaterializedEpoch =
            g_t6ValidationFailureMaterializedEpoch;
        out->unresolvedSnapshotSeq =
            g_t6ValidationFailureSnapshotSeq;
        out->unresolvedRequiredSeq =
            g_t6ValidationFailureRequiredSeq;
        out->unresolvedSourceId =
            g_t6ValidationFailureSourceId;
        out->unresolvedMapId = g_t6ValidationFailureMapId;
#endif
        return;
    }
    out->unresolvedReason = T6_REASON_NONE;
    out->result = VRAM_FRESH_MATERIALIZED;
}

/*
 * One-shot, read-only evidence for the CPU-newer negative gate.  This runs
 * after FinishedVRAMWrite() has assigned the A0 CPU epoch and before its
 * synchronous CheckWriteUpdate(), so an older mapped EFB/snapshot is still
 * observable.  Only one 16x16 tile is passed through the same generic shadow
 * decision used by MoveImage; no capture, merge, epoch commit or cache
 * invalidation is performed here.
 */
typedef struct {
    int found;
    int qualified;
    int tx, ty;
    int mapping;
    uint32_t mapId;
    uint64_t provenanceSeq;
    uint64_t cpuWriteEpoch;
    uint64_t materializedColorEpoch;
    uint64_t physicalEfbSeq;
    uint64_t snapshotSeq;
    VramFreshResult result;
    int unresolvedReason;
    int hazardTiles;
    int captureRequired;
    int wouldCapture;
    int capturePlanSlot;
    int captureBufferReady;
    unsigned int requiredTiles;
    uint32_t requiredMapId;
    uint64_t requiredSeq;
} T6CpuNewerProbeEvidence;

static VramMoveShadowDecision g_t6CpuNewerProbeShadow;

static uint64_t T6CpuNewerRawSnapshotSeqForTile(
    int tx, int ty, uint32_t mapId)
{
    uint64_t newest = 0;
    int i;

    for (i = 0; i < 2; i++)
    {
        const GXFrameSnapshot *snap = &g_snapshot[i];
        const EfbTileState *st = &snap->efbTile[ty & 31][tx & 63];

        if (snap->valid && snap->map_id == mapId &&
            st->cover == EFB_TILE_FULL && st->seq != 0 &&
            SnapshotCoversTile(snap, tx, ty) &&
            SnapshotExactRectCoversTile(snap, tx, ty) &&
            st->seq > newest)
            newest = st->seq;
    }
    return newest;
}

static int T6CpuNewerProbeForA0(
    int x, int y, int w, int h, uint64_t provenanceSeq,
    T6CpuNewerProbeEvidence *out)
{
    VramReadDependency dep;
    unsigned int i;

    if (out == NULL)
        return 0;
    memset(out, 0, sizeof(*out));
    out->tx = -1;
    out->ty = -1;
    out->mapping = (int)MAPPING_UNKNOWN;
    out->result = VRAM_FRESH_UNRESOLVED;
    out->unresolvedReason = T6_REASON_CAPTURE_FAIL;
    out->provenanceSeq = provenanceSeq;

    if (provenanceSeq == 0 || iGPUHeight != 512 ||
        g_t6WorkspaceDepth != 0 || g_vramReadBarrierDepth != 0 ||
        !BuildMoveSourceDependency(x, y, w, h, 1024, 512, &dep))
        return 0;

    for (i = 0; i < dep.count; i++)
    {
        const VramRect *r = &dep.rect[i];
        int tx0 = r->x0 >> 4;
        int tx1 = (r->x1 - 1) >> 4;
        int ty0 = r->y0 >> 4;
        int ty1 = (r->y1 - 1) >> 4;
        int tx, ty;

        for (ty = ty0; ty <= ty1; ty++)
            for (tx = tx0; tx <= tx1; tx++)
            {
                T6PhysicalTileState phys;
                MappingKind mapping = ClassifyTileMapping(tx, ty);
                uint32_t mapId;
                uint64_t cpu, materialized, physicalSeq = 0, snapshotSeq;

                if (mapping == MAPPING_CURRENT)
                    mapId = g_activeMap.map_id;
                else if (mapping == MAPPING_PREVIOUS)
                    mapId = g_prevMapId;
                else
                    continue;
                if (mapId == INVALID_MAP_ID)
                    continue;

                cpu = g_psxWriteTileEpoch[ty & 31][tx & 63];
                materialized =
                    g_psxMaterializedColorTileEpoch[ty & 31][tx & 63];
                if (cpu != provenanceSeq || cpu <= materialized)
                    continue;

                memset(&phys, 0, sizeof(phys));
                if (TilePhysicalEfbState(tx, ty, mapping, mapId, &phys) &&
                    TilePhysicalOwnsTargetMap(&phys, mapping, mapId))
                    physicalSeq = phys.seq;
                snapshotSeq =
                    T6CpuNewerRawSnapshotSeqForTile(tx, ty, mapId);
                if ((physicalSeq > snapshotSeq ? physicalSeq : snapshotSeq) ==
                        0 ||
                    cpu <= (physicalSeq > snapshotSeq ?
                            physicalSeq : snapshotSeq))
                    continue;

                T6VramMoveShadowDecisionForRect(
                    tx << 4, ty << 4,
                    VRAM_READBACK_TILE_SIZE, VRAM_READBACK_TILE_SIZE,
                    &g_t6CpuNewerProbeShadow);

                out->found = 1;
                out->tx = tx;
                out->ty = ty;
                out->mapping = (int)mapping;
                out->mapId = mapId;
                out->cpuWriteEpoch = cpu;
                out->materializedColorEpoch = materialized;
                out->physicalEfbSeq = physicalSeq;
                out->snapshotSeq = snapshotSeq;
                out->result = g_t6CpuNewerProbeShadow.result;
                out->unresolvedReason =
                    g_t6CpuNewerProbeShadow.unresolvedReason;
                out->hazardTiles = g_t6CpuNewerProbeShadow.hazardTiles;
                out->captureRequired =
                    g_t6CpuNewerProbeShadow.captureRequired;
                out->wouldCapture =
                    g_t6CpuNewerProbeShadow.wouldCapture;
                out->capturePlanSlot =
                    g_t6CpuNewerProbeShadow.capturePlanSlot;
                out->captureBufferReady =
                    g_t6CpuNewerProbeShadow.captureBufferReady;
                out->requiredTiles =
                    g_t6CpuNewerProbeShadow.requiredTiles;
                out->requiredMapId =
                    g_t6CpuNewerProbeShadow.requiredMapId;
                out->requiredSeq =
                    g_t6CpuNewerProbeShadow.requiredSeq;
                out->qualified = T6CpuNewerProbeQualifies(
                    provenanceSeq, cpu, materialized,
                    physicalSeq, snapshotSeq,
                    &g_t6CpuNewerProbeShadow);
                return 1;
            }
    }
    return 0;
}
#endif /* T6_BARRIER_DIAG */

#endif /* GPU_VRAM_READBACK_INC */
