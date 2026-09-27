#ifndef GPU_VRAM_TILING_H
#define GPU_VRAM_TILING_H

#include <stddef.h>
#include <stdint.h>

#if defined(GLES_VRAM_LR_TILING_S1_TEST) && \
    !defined(GLES_VRAM_LR_TILING_EXPERIMENT)
#error "GLES_VRAM_LR_TILING_S1_TEST requires GLES_VRAM_LR_TILING_EXPERIMENT"
#endif
#if defined(GLES_VRAM_LR_TILING_S2_EXPERIMENT) && \
    !defined(GLES_VRAM_LR_TILING_EXPERIMENT)
#error "GLES_VRAM_LR_TILING_S2_EXPERIMENT requires GLES_VRAM_LR_TILING_EXPERIMENT"
#endif
#if defined(GLES_VRAM_LR_TILING_S2_TEST) && \
    !defined(GLES_VRAM_LR_TILING_S2_EXPERIMENT)
#error "GLES_VRAM_LR_TILING_S2_TEST requires GLES_VRAM_LR_TILING_S2_EXPERIMENT"
#endif
#if defined(GLES_VRAM_LR_TILING_S3_EXPERIMENT) && \
    !defined(GLES_VRAM_LR_TILING_S2_EXPERIMENT)
#error "GLES_VRAM_LR_TILING_S3_EXPERIMENT requires GLES_VRAM_LR_TILING_S2_EXPERIMENT"
#endif
#if defined(GLES_VRAM_LR_TILING_S3_TEST) && \
    !defined(GLES_VRAM_LR_TILING_S3_EXPERIMENT)
#error "GLES_VRAM_LR_TILING_S3_TEST requires GLES_VRAM_LR_TILING_S3_EXPERIMENT"
#endif
#if defined(GLES_VRAM_LR_TILING_S4_EXPERIMENT) && \
    !defined(GLES_VRAM_LR_TILING_S3_EXPERIMENT)
#error "GLES_VRAM_LR_TILING_S4_EXPERIMENT requires GLES_VRAM_LR_TILING_S3_EXPERIMENT"
#endif
#if defined(GLES_VRAM_LR_TILING_S4_TEST) && \
    !defined(GLES_VRAM_LR_TILING_S4_EXPERIMENT)
#error "GLES_VRAM_LR_TILING_S4_TEST requires GLES_VRAM_LR_TILING_S4_EXPERIMENT"
#endif
#if defined(GLES_VRAM_LR_TILING_S5_EXPERIMENT) && \
    !defined(GLES_VRAM_LR_TILING_S4_EXPERIMENT)
#error "GLES_VRAM_LR_TILING_S5_EXPERIMENT requires GLES_VRAM_LR_TILING_S4_EXPERIMENT"
#endif
#if defined(GLES_VRAM_LR_TILING_S6_EXPERIMENT) && \
    !defined(GLES_VRAM_LR_TILING_S5_EXPERIMENT)
#error "GLES_VRAM_LR_TILING_S6_EXPERIMENT requires GLES_VRAM_LR_TILING_S5_EXPERIMENT"
#endif
#if defined(GLES_VRAM_LR_TILING_S7_EXPERIMENT) && \
    !defined(GLES_VRAM_LR_TILING_S6_EXPERIMENT)
#error "GLES_VRAM_LR_TILING_S7_EXPERIMENT requires GLES_VRAM_LR_TILING_S6_EXPERIMENT"
#endif
#if defined(GLES_VRAM_LR_TILING_S7_ANIM_DIAG) && \
    !defined(GLES_VRAM_LR_TILING_S7_EXPERIMENT)
#error "GLES_VRAM_LR_TILING_S7_ANIM_DIAG requires GLES_VRAM_LR_TILING_S7_EXPERIMENT"
#endif
#if defined(GLES_VRAM_LR_TILING_S7_PIPELINE_DIAG) && \
    !defined(GLES_VRAM_LR_TILING_S7_ANIM_DIAG)
#error "GLES_VRAM_LR_TILING_S7_PIPELINE_DIAG requires GLES_VRAM_LR_TILING_S7_ANIM_DIAG"
#endif
#if defined(GLES_VRAM_LR_TILING_S7_PRESENT_WAIT_TEST) && \
    !defined(GLES_VRAM_LR_TILING_S7_EXPERIMENT)
#error "GLES_VRAM_LR_TILING_S7_PRESENT_WAIT_TEST requires GLES_VRAM_LR_TILING_S7_EXPERIMENT"
#endif
#if defined(GLES_VRAM_LR_TILING_S7_DIRECT_BACKING_TEST) && \
    !defined(GLES_VRAM_LR_TILING_S7_EXPERIMENT)
#error "GLES_VRAM_LR_TILING_S7_DIRECT_BACKING_TEST requires GLES_VRAM_LR_TILING_S7_EXPERIMENT"
#endif
#if defined(GLES_VRAM_LR_TILING_S7_STITCHED_PRESENT_TEST) && \
    !defined(GLES_VRAM_LR_TILING_S7_EXPERIMENT)
#error "GLES_VRAM_LR_TILING_S7_STITCHED_PRESENT_TEST requires GLES_VRAM_LR_TILING_S7_EXPERIMENT"
#endif
#if defined(VRAM_TILING_DIAG_ONLY) && \
    !defined(GLES_VRAM_LR_TILING_S5_EXPERIMENT)
#error "VRAM_TILING_DIAG_ONLY requires GLES_VRAM_LR_TILING_S5_EXPERIMENT"
#endif
#if defined(GLES_VRAM_LR_TILING_S1_TEST) && \
    (defined(GLES_VRAM_LR_TILING_S2_TEST) || \
     defined(GLES_VRAM_LR_TILING_S3_TEST))
#error "S1 and later visual tests are mutually exclusive"
#endif
#if defined(GLES_VRAM_LR_TILING_S2_TEST) && \
    defined(GLES_VRAM_LR_TILING_S3_TEST)
#error "S2 and S3 visual tests are mutually exclusive"
#endif
#if defined(GLES_VRAM_LR_TILING_S4_TEST) && \
    (defined(GLES_VRAM_LR_TILING_S2_TEST) || \
     defined(GLES_VRAM_LR_TILING_S3_TEST))
#error "S4 and earlier visual tests are mutually exclusive"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define GLES_VRAM_WIDTH       1024
#define GLES_VRAM_HEIGHT       512
#define GLES_VRAM_LEFT_WIDTH   640
#define GLES_VRAM_RIGHT_X      640
#define GLES_VRAM_RIGHT_WIDTH  384

#define GLES_VRAM_MAX_WRAP_PIECES 4
#define GLES_VRAM_MAX_TILE_SPANS  8
#define GLES_VRAM_MAX_DISPLAY_SEGMENTS GLES_VRAM_MAX_TILE_SPANS

typedef enum GlesVramTileIdTag
{
    GLES_VRAM_TILE_NONE  = -1,
    GLES_VRAM_TILE_LEFT  = 0,
    GLES_VRAM_TILE_RIGHT = 1
} GlesVramTileId;

typedef struct GlesVramRectTag
{
    int x;
    int y;
    int width;
    int height;
} GlesVramRect;

/* One non-wrapping piece of a logical command rectangle.  commandX/Y are
 * offsets from the first pixel of the original command and preserve transfer
 * order after an X/Y wrap. */
typedef struct GlesVramRectPieceTag
{
    GlesVramRect rect;
    int commandX;
    int commandY;
} GlesVramRectPiece;

typedef struct GlesVramTileSpanTag
{
    GlesVramTileId tile;
    GlesVramRect vramRect;
    GlesVramRect localRect;
    int commandX;
    int commandY;
} GlesVramTileSpan;

typedef struct GlesVramPointTag
{
    int x;
    int y;
} GlesVramPoint;

typedef struct GlesVramPrimitivePlanTag
{
    GlesVramRect writeRect;
    GlesVramTileSpan span[GLES_VRAM_MAX_TILE_SPANS];
    int spanCount;
    int isFill;
} GlesVramPrimitivePlan;

typedef struct GlesVramDisplaySegmentTag
{
    GlesVramTileId tile;
    GlesVramRect sourceRect;
    GlesVramRect outputRect;
} GlesVramDisplaySegment;

typedef struct GlesVramDisplayPlanTag
{
    GlesVramRect displayRect;
    GlesVramDisplaySegment segment[GLES_VRAM_MAX_DISPLAY_SEGMENTS];
    int segmentCount;
} GlesVramDisplayPlan;

typedef struct GlesVramS5StatsTag
{
    uint32_t moveCount;
    uint32_t c0Count;
    uint32_t resolveCalls;
    uint32_t resolveWaits;
    uint32_t resolvedBlocks;
} GlesVramS5Stats;

#ifdef GLES_VRAM_LR_TILING_S7_ANIM_DIAG
/* Per-present work summary used only by the short-lived animation diagnostic
 * build.  Bounds are absolute, half-open PS VRAM rectangles. */
typedef struct GlesVramS7AnimStatsTag
{
    uint32_t uploadCalls[2];
    uint32_t uploadBlocks[2];
    uint32_t uploadRuns[2];
    uint32_t uploadCommits[2];
    uint32_t uploadCommitFailures[2];
    uint32_t uploadSourceHash[2];
    uint32_t uploadStagingHash[2];
    GlesVramRect uploadBounds[2];
    uint32_t gpuPasses[2];
    GlesVramRect gpuBounds[2];
    uint32_t tileSelects[2];
    uint32_t saveCopies[2];
    uint32_t restores[2];
    uint32_t clears[2];
#ifdef GLES_VRAM_LR_TILING_S7_PIPELINE_DIAG
    uint32_t pipelineValid;
    uint32_t sourceHash;
    uint32_t sourceNonBlack;
    uint32_t cpuHash;
    uint32_t cpuComparable;
    uint32_t cpuMismatch;
    GlesVramRect cpuMismatchBounds;
    uint32_t efbHash;
    uint32_t efbNonBlack;
    uint32_t efbMismatch;
    GlesVramRect efbMismatchBounds;
    uint32_t ownerCpuOnly;
    uint32_t ownerGpuOnly;
    uint32_t ownerEqual;
    uint32_t ownerInvalid;
    uint32_t ownerNeedsUpload;
    uint32_t segmentCount;
    uint32_t segmentHash[GLES_VRAM_MAX_DISPLAY_SEGMENTS];
    uint32_t segmentNonBlack[GLES_VRAM_MAX_DISPLAY_SEGMENTS];
    uint32_t efbQuadrantHash[4];
    uint32_t efbQuadrantNonBlack[4];
#endif
} GlesVramS7AnimStats;

#ifdef GLES_VRAM_LR_TILING_S7_PIPELINE_DIAG
typedef struct GlesVramS7XfbStatsTag
{
    uint32_t valid;
    uint32_t frame;
    uint32_t hash;
    uint32_t lumaHash;
    uint32_t lumaActive;
    uint32_t byteCount;
    uint32_t width;
    uint32_t height;
    uint32_t stridePixels;
    uint8_t lumaMin;
    uint8_t lumaMax;
} GlesVramS7XfbStats;
#endif
#endif

/* Decode GP0 transfer extents.  A zero encoded width/height means the full
 * 1024/512 dimension after the hardware masks are applied. */
void GlesVramDecodeTransferSize(uint32_t sizeWord,
                                int *width, int *height);

/* Split a positive rectangle of at most 1024x512 at the PS VRAM wrap edges.
 * The input origin may be outside the canonical range and is normalized.
 * Returns zero for invalid input. */
int GlesVramSplitWrappedRect(const GlesVramRect *input,
                             GlesVramRectPiece output[
                                 GLES_VRAM_MAX_WRAP_PIECES]);

/* Split a logical rectangle first at wrap edges, then at X=640.  Every output
 * span has both absolute PS VRAM coordinates and tile-local coordinates. */
int GlesVramBuildTileSpans(const GlesVramRect *input,
                           GlesVramTileSpan output[
                               GLES_VRAM_MAX_TILE_SPANS]);

/* Build source-tile and unscaled output rectangles for a PS display window.
 * outputRect is relative to the requested display origin and therefore keeps
 * X/Y wrap pieces adjacent in presentation order. */
int GlesVramBuildDisplayPlan(const GlesVramRect *displayRect,
                             GlesVramDisplayPlan *plan);

/* Build the absolute PS VRAM write range for GP0 Fill or a drawing primitive.
 * packet points at little-endian GP0 words, as stored in gpuDataM.  Both
 * opaque and semi-transparent primitives must enter the tile manager;
 * non-drawing commands return zero. */
int GlesVramBuildPrimitivePlan(unsigned char command,
                               const void *packet, int packetWords,
                               const GlesVramRect *drawArea,
                               const GlesVramPoint *drawOffset,
                               GlesVramPrimitivePlan *plan);

#ifdef GLES_VRAM_LR_TILING_EXPERIMENT
int GlesVramTilingInitialize(void);
void GlesVramTilingReset(void);
void GlesVramTilingShutdown(void);
size_t GlesVramTilingAllocatedBytes(void);
int GlesVramTilingIsInitialized(void);
int GlesVramTilingSelectTile(GlesVramTileId tile);
void GlesVramTilingMarkActiveDirty(void);
void GlesVramTilingDiscardActive(void);
GlesVramTileId GlesVramTilingActiveTile(void);
#ifdef GLES_VRAM_LR_TILING_S2_EXPERIMENT
int GlesVramTilingS2BeginCommand(unsigned char command,
                                 const void *packet, int packetWords,
                                 const GlesVramRect *drawArea,
                                 const GlesVramPoint *drawOffset,
                                 const GlesVramRect *frontDisplay);
void GlesVramTilingS2EndCommand(void);
int GlesVramTilingS2CommandActive(void);
int GlesVramTilingS2PassCount(void);
int GlesVramTilingS2PreparePass(int passIndex,
                                GlesVramTileSpan *span,
                                int *isFill);
void GlesVramTilingS2NotifyDrawSubmitted(void);
void GlesVramTilingS2FinishPass(void);
void GlesVramTilingS2SaveAndDiscard(void);
void GlesVramTilingS2DrawDebugComposite(void);
#ifdef GLES_VRAM_LR_TILING_S2_TEST
int GlesVramTilingS2VisualTestReady(void);
void GlesVramTilingS2SetVisualTestReady(void);
#endif
#ifdef GLES_VRAM_LR_TILING_S3_EXPERIMENT
int GlesVramTilingS3PrepareCpuWrite(unsigned short *cpuVram,
                                    const GlesVramRect *rect);
uint64_t GlesVramTilingS3FinishCpuWrite(const GlesVramRect *rect);
int GlesVramTilingS3RectNeedsCpuUpload(const GlesVramRect *rect);
#ifdef GLES_VRAM_LR_TILING_S3_TEST
int GlesVramTilingS3VisualTestReady(void);
void GlesVramTilingS3SetVisualTestReady(void);
void GlesVramTilingS3CaptureEfbTestDiagnostics(void);
void GlesVramTilingS3CaptureTestDiagnostics(void);
#endif
#ifdef GLES_VRAM_LR_TILING_S4_EXPERIMENT
int GlesVramTilingS4Present(const GlesVramRect *displayRect,
                            int targetWidth, int targetHeight);
#ifdef GLES_VRAM_LR_TILING_S4_TEST
void GlesVramTilingS4DrawVisualTest(void);
#endif
#ifdef GLES_VRAM_LR_TILING_S5_EXPERIMENT
/* Resolve GPU-new source blocks once, then keep psxVuw current for all later
 * CPU consumers of the same generation. */
int GlesVramTilingS5EnsureCpuCurrent(unsigned short *cpuVram,
                                    const GlesVramRect *rect);
int GlesVramTilingS5MoveImage(unsigned short *cpuVram,
                             const GlesVramRect *source,
                             const GlesVramRect *destination,
                             uint16_t setMask);
int GlesVramTilingS5PrepareRead(unsigned short *cpuVram,
                               const GlesVramRect *rect);
void GlesVramTilingS5GetStats(GlesVramS5Stats *stats);
#ifdef GLES_VRAM_LR_TILING_S7_EXPERIMENT
/* A primitive written to the page being scanned out is visible before the
 * following GP1 page switch.  Select that just-updated front page for this
 * host presentation instead of losing the interval at the switch boundary. */
int GlesVramTilingS7SelectPresentation(
    const GlesVramRect *requestedDisplay,
    GlesVramRect *selectedDisplay);
void GlesVramTilingS7DiscardFrontPresentation(void);
#ifdef GLES_VRAM_LR_TILING_S7_ANIM_DIAG
/* Return all tile work accumulated since the preceding call, then clear it. */
void GlesVramTilingS7AnimGetAndResetStats(GlesVramS7AnimStats *stats);
#ifdef GLES_VRAM_LR_TILING_S7_PIPELINE_DIAG
void GlesVramTilingS7DiagSetPresentFrame(uint32_t frame);
void GlesVramTilingS7DiagBeginXfb(void *pixels, int width, int height,
                                  int stridePixels);
void GlesVramTilingS7DiagCompleteXfb(void);
int GlesVramTilingS7DiagGetXfbStats(GlesVramS7XfbStats *stats);
#endif
#endif
#endif
#endif
#endif
#endif
#endif
#ifdef GLES_VRAM_LR_TILING_S1_TEST
void GlesVramTilingRunS1VisualTest(void);
#endif
#else
/* The experiment is deliberately default-off.  Inline no-ops let LTO remove
 * every lifecycle call from normal builds. */
static inline int GlesVramTilingInitialize(void) { return 1; }
static inline void GlesVramTilingReset(void) {}
static inline void GlesVramTilingShutdown(void) {}
static inline size_t GlesVramTilingAllocatedBytes(void) { return 0; }
static inline int GlesVramTilingIsInitialized(void) { return 0; }
static inline int GlesVramTilingSelectTile(GlesVramTileId tile)
{
    (void)tile;
    return 0;
}
static inline void GlesVramTilingMarkActiveDirty(void) {}
static inline void GlesVramTilingDiscardActive(void) {}
static inline GlesVramTileId GlesVramTilingActiveTile(void)
{
    return GLES_VRAM_TILE_NONE;
}
#endif

#ifdef __cplusplus
}
#endif

#endif
