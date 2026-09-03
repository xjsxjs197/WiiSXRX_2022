/***************************************************************************
                          test_t6_barrier.c
                             -------------------
    Host-side tests for the T6-A pure texture read barrier helpers.
    Not part of the Wii build; compile with a host C compiler:
        cc -std=c99 -Wall -Wextra -Werror -I../GlesGpu \
           test_t6_barrier.c -o test_t6_barrier
 ***************************************************************************/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../GlesGpu/gpuTextureReadBarrier.h"
#include "../GlesGpu/gpuVramReadbackScope.h"

static int s_failures = 0;
static void expect_count(int got, int expected);
static void expect_bool(int got, int expected, const char *what);

static void test_dc2_readback_scope(void)
{
    static const int sourceX[] = {0, 64, 128, 192, 256};
    DC2ReadbackScope scope = {0, 0};
    unsigned int i;

    expect_bool(DC2ReadbackScopeIsTriggerC0(0, 256, 320, 240), 1,
                "DC2 underwater C0 accepted");
    expect_bool(DC2ReadbackScopeIsTriggerC0(0, 0, 320, 240), 0,
                "DC2 unrelated C0 y rejected");
    expect_bool(DC2ReadbackScopeIsTriggerC0(0, 256, 64, 240), 0,
                "DC2 unrelated C0 width rejected");

    for (i = 0; i < sizeof(sourceX) / sizeof(sourceX[0]); i++)
    {
        expect_bool(DC2ReadbackScopeIsUnderwaterMove(
                        sourceX[i], 0, 448, 256, 64, 240), 1,
                    "DC2 upper underwater strip accepted");
        expect_bool(DC2ReadbackScopeIsUnderwaterMove(
                        sourceX[i], 256, 448, 256, 64, 240), 1,
                    "DC2 lower underwater strip accepted");
    }
    expect_bool(DC2ReadbackScopeIsUnderwaterMove(
                    320, 0, 448, 256, 64, 240), 0,
                "DC2 out-of-range strip rejected");
    expect_bool(DC2ReadbackScopeIsUnderwaterMove(
                    64, 0, 384, 256, 64, 240), 0,
                "DC2 wrong strip destination rejected");
    expect_bool(DC2ReadbackScopeIsUnderwaterMove(
                    64, 0, 448, 256, 32, 240), 0,
                "DC2 wrong strip width rejected");

    expect_bool(DC2ReadbackScopeAllowsWork(1, &scope), 0,
                "DC2 scope initially closed");
    expect_bool(DC2ReadbackScopeAllowsWork(0, &scope), 1,
                "non-DC2 readback remains enabled");
    DC2ReadbackScopeArm(&scope);
    expect_count((int)scope.holdFrames, DC2_READBACK_SCOPE_HOLD_FRAMES);
    expect_count((int)scope.generation, 1);
    expect_bool(DC2ReadbackScopeAllowsWork(1, &scope), 1,
                "DC2 scope opens after trigger");
    for (i = 1; i < DC2_READBACK_SCOPE_HOLD_FRAMES; i++)
        expect_bool(DC2ReadbackScopeAdvanceFrame(&scope), 0,
                    "DC2 scope remains open before timeout");
    DC2ReadbackScopeArm(&scope);
    expect_count((int)scope.holdFrames, DC2_READBACK_SCOPE_HOLD_FRAMES);
    expect_count((int)scope.generation, 1);
    for (i = 1; i < DC2_READBACK_SCOPE_HOLD_FRAMES; i++)
        DC2ReadbackScopeAdvanceFrame(&scope);
    expect_bool(DC2ReadbackScopeAdvanceFrame(&scope), 1,
                "DC2 scope reports timeout transition");
    expect_bool(DC2ReadbackScopeActive(&scope), 0,
                "DC2 scope closes at timeout");
    DC2ReadbackScopeArm(&scope);
    expect_count((int)scope.generation, 2);
    expect_bool(DC2ReadbackScopeActive(&scope), 1,
                "DC2 scope reopens with a new generation");
    expect_bool(DC2ReadbackScopeCaptureIsCurrent(1, &scope, 1), 0,
                "DC2 late prior-generation capture rejected");
    expect_bool(DC2ReadbackScopeCaptureIsCurrent(1, &scope, 2), 1,
                "DC2 current-generation capture accepted");
    expect_bool(DC2ReadbackScopeCaptureIsCurrent(0, &scope, 0), 1,
                "non-DC2 capture ignores scope generation");
    scope.holdFrames = 0;
    expect_bool(DC2ReadbackScopeAdvanceFrame(&scope), 0,
                "closed DC2 scope stays closed");
}

static void expect_rect(const VramRect *r, int x0, int y0, int x1, int y1)
{
    if (r->x0 != x0 || r->y0 != y0 || r->x1 != x1 || r->y1 != y1)
    {
        printf("FAIL rect got %d,%d %d,%d expected %d,%d %d,%d\n",
               r->x0, r->y0, r->x1, r->y1, x0, y0, x1, y1);
        s_failures++;
    }
}

static void expect_count(int got, int expected)
{
    if (got != expected)
    {
        printf("FAIL count got %d expected %d\n", got, expected);
        s_failures++;
    }
}

static void expect_bool(int got, int expected, const char *what)
{
    if (got != expected)
    {
        printf("FAIL %s got %d expected %d\n", what, got, expected);
        s_failures++;
    }
}

static void expect_u64(uint64_t got, uint64_t expected, const char *what)
{
    if (got != expected)
    {
        printf("FAIL %s got %llu expected %llu\n", what,
               (unsigned long long)got, (unsigned long long)expected);
        s_failures++;
    }
}

static int dependency_contains_word(const VramReadDependency *dep,
                                    int x, int y)
{
    int i;

    for (i = 0; i < (int)dep->count; i++)
        if (dep->rect[i].x0 <= x && x < dep->rect[i].x1 &&
            dep->rect[i].y0 <= y && y < dep->rect[i].y1)
            return 1;

    return 0;
}

static void test_epoch(void)
{
    expect_u64(PsxVuwColorTileEpoch(10, 0), 10, "cpu newer");
    expect_u64(PsxVuwColorTileEpoch(10, 11), 11, "materialized newer");
    expect_u64(PsxVuwColorTileEpoch(12, 11), 12, "cpu wins after materialize");
    expect_u64(PsxVuwColorTileEpoch(0, 0), 0, "zero epoch");
}

static void test_vram_write_payload_gate(void)
{
    expect_bool(T6VramWritePayloadActive(0, 0), 0,
                "normal mode without A0");
    expect_bool(T6VramWritePayloadActive(0, 1), 0,
                "legacy mode cancellation");
    expect_bool(T6VramWritePayloadActive(1, 0), 0,
                "GP1 DMA direction is not A0");
    expect_bool(T6VramWritePayloadActive(1, 1), 1,
                "decoded A0 payload active");
    expect_bool(T6VramWriteModeNeedsNormalize(0, 0), 0,
                "normal parser mode stays normal");
    expect_bool(T6VramWriteModeNeedsNormalize(0, 1), 0,
                "cancelled transfer needs no normalization");
    expect_bool(T6VramWriteModeNeedsNormalize(1, 0), 1,
                "unarmed legacy DMA mode normalizes once");
    expect_bool(T6VramWriteModeNeedsNormalize(1, 1), 0,
                "armed A0 payload must not normalize");
}

static void test_potential_tile_prefilter(void)
{
    VramReadDependency dep;
    uint32_t potential[T6_VRAM_TILE_Y][2];
    int tx, ty, propertyOk = 1;

    memset(&dep, 0, sizeof(dep));
    memset(potential, 0, sizeof(potential));
    dep.count = 1;
    dep.rect[0].x0 = 32;
    dep.rect[0].y0 = 48;
    dep.rect[0].x1 = 80;
    dep.rect[0].y1 = 80;

    expect_bool(T6DependencyIntersectsPotentialTiles(&dep, potential), 0,
                "potential prefilter empty");
    potential[3][0] = 1u << 1; /* x=16..31: immediately left. */
    expect_bool(T6DependencyIntersectsPotentialTiles(&dep, potential), 0,
                "potential prefilter adjacent");
    potential[3][0] |= 1u << 2; /* x=32..47: first covered tile. */
    expect_bool(T6DependencyIntersectsPotentialTiles(&dep, potential), 1,
                "potential prefilter first tile");

    memset(potential, 0, sizeof(potential));
    potential[4][1] = 1u << (33 - 32);
    dep.rect[0].x0 = 512;
    dep.rect[0].x1 = 544;
    expect_bool(T6DependencyIntersectsPotentialTiles(&dep, potential), 1,
                "potential prefilter upper word");

    dep.rect[0].x1 = 1025;
    expect_bool(T6DependencyIntersectsPotentialTiles(&dep, potential), 1,
                "potential prefilter malformed fail closed");

    /* Snapshot capture may make a tile observable without a new draw.  The
     * merge must restore x=0 bits, ignore stale/partial tiles and never clear
     * an existing conservative positive. */
    memset(potential, 0, sizeof(potential));
    dep.rect[0].x0 = 0;
    dep.rect[0].y0 = 0;
    dep.rect[0].x1 = 64;
    dep.rect[0].y1 = 240;
    T6PotentialMergeSnapshotTile(potential, 0, 0, 1, 20, 10, 15);
    expect_bool(T6DependencyIntersectsPotentialTiles(&dep, potential), 1,
                "captured x0 newer tile restores potential bit");
    T6PotentialMergeSnapshotTile(potential, 0, 0, 0, 100, 0, 0);
    T6PotentialMergeSnapshotTile(potential, 0, 0, 1, 5, 10, 15);
    expect_bool((potential[0][0] & 1u) != 0, 1,
                "snapshot merge never clears conservative positive");

    /* Property gate: every full snapshot tile newer than the combined PSX
     * color epoch must be represented in the summary, across both words. */
    memset(potential, 0, sizeof(potential));
    for (ty = 0; ty < T6_VRAM_TILE_Y; ty++)
        for (tx = 0; tx < T6_VRAM_TILE_X; tx++)
        {
            uint64_t cpu = (uint64_t)(tx + ty);
            uint64_t mat = (uint64_t)(tx * 2 + (ty & 3));
            uint64_t color = cpu > mat ? cpu : mat;
            int full = ((tx + ty) % 3) != 0;
            uint64_t snap = color + (((tx ^ ty) & 1) ? 1u : 0u);

            T6PotentialMergeSnapshotTile(
                potential, tx, ty, full, snap, cpu, mat);
            if (full && snap > color &&
                !(potential[ty][tx >> 5] & (1u << (tx & 31))))
                propertyOk = 0;
        }
    expect_bool(propertyOk, 1,
                "snapshot full-scan hazards are a subset of potential");
}

static void test_append(void)
{
    VramReadDependency dep;
    VramRect r = { 0, 0, 16, 16 };
    VramRect empty = { 0, 0, 0, 0 };
    int i, ok;

    memset(&dep, 0, sizeof(dep));
    ok = VramReadDependencyAppend(&dep, &empty);
    expect_bool(ok, 0, "empty rect rejected");

    for (i = 0; i < 12; i++)
        expect_bool(VramReadDependencyAppend(&dep, &r), 1,
                    "capacity append");
    expect_count((int)dep.count, 12);
    expect_bool(VramReadDependencyAppend(&dep, &r), 0,
                "capacity overflow rejected");
}

static void test_source_linear(void)
{
    VramRect out[2];
    int n;

    n = SourceLinearRects(10, 20, 5, 7, 1024, 512, out);
    expect_count(n, 1);
    if (n > 0)
        expect_rect(&out[0], 10, 20, 15, 27);

    n = SourceLinearRects(1022, 10, 4, 2, 1024, 512, out);
    expect_count(n, 2);
    if (n >= 1)
        expect_rect(&out[0], 1022, 10, 1024, 12);
    if (n >= 2)
        expect_rect(&out[1], 0, 11, 2, 13);
}

static void test_standard_word_rect(void)
{
    VramRect r;

    expect_bool(StandardTextureSourceWordRect(0, 0, 3, 4, 0, 1, &r), 1,
                "mode0 build");
    expect_rect(&r, 0, 0, 2, 2);

    expect_bool(StandardTextureSourceWordRect(0, 1, 1, 2, 0, 0, &r), 1,
                "mode1 build");
    expect_rect(&r, 0, 0, 2, 1);

    expect_bool(StandardTextureSourceWordRect(0, 2, 0, 255, 0, 255, &r), 1,
                "mode2 build");
    expect_rect(&r, 0, 0, 256, 256);

    expect_bool(StandardTextureSourceWordRect(0, 0, 4, 3, 0, 0, &r), 0,
                "invalid uv rejected");
}

static void test_standard_dependency(void)
{
    VramReadDependency dep;
    int ok;

    memset(&dep, 0, sizeof(dep));
    ok = BuildStandardTextureDependency(0, 0, 0, 0x1FF, 1024, 512,
                                        3, 4, 0, 1, 0, &dep);
    expect_bool(ok, 1, "mode0 dependency");
    expect_count((int)dep.count, 4);
    if (dep.count > 0)
        expect_rect(&dep.rect[0], 0, 0, 2, 2);

    memset(&dep, 0, sizeof(dep));
    ok = BuildStandardTextureDependency(15, 1, 0x3F, 0x1FF, 1024, 512,
                                        0, 255, 0, 255, 0, &dep);
    expect_bool(ok, 1, "wrap dependency");
    expect_count((int)dep.count, 8);
    if (dep.count > 0)
        expect_rect(&dep.rect[0], 960, 0, 1024, 256);
    if (dep.count > 1)
        expect_rect(&dep.rect[1], 0, 0, 64, 256);

    memset(&dep, 0, sizeof(dep));
    ok = BuildStandardTextureDependency(14, 2, 0, 0x1FF, 1024, 512,
                                        0, 255, 0, 255, 0, &dep);
    expect_bool(ok, 1, "mode2 dependency");
    expect_count((int)dep.count, 4);
    if (dep.count > 0)
        expect_rect(&dep.rect[0], 896, 0, 1024, 256);
    if (dep.count > 1)
        expect_rect(&dep.rect[1], 0, 0, 128, 256);

    memset(&dep, 0, sizeof(dep));
    ok = BuildStandardTextureDependency(0, 0, 0, 0x1FF, 1024, 512,
                                        0, 255, 0, 255, 1, &dep);
    expect_bool(ok, 1, "interleaved dependency");
    expect_count((int)dep.count, 4);
    if (dep.count > 0)
        expect_rect(&dep.rect[0], 0, 0, 64, 256);
}

static void test_window_dependency(void)
{
    VramReadDependency dep;
    int ok;

    memset(&dep, 0, sizeof(dep));
    ok = BuildWindowTextureDependency(15, 1, 0x3F, 0x1FF, 1024, 512, &dep);
    expect_bool(ok, 1, "window wrap dependency");
    expect_count((int)dep.count, 8);

    memset(&dep, 0, sizeof(dep));
    ok = BuildWindowTextureDependency(14, 2, 0, 0x1FF, 1024, 512, &dep);
    expect_bool(ok, 1, "window mode2 dependency");
    expect_count((int)dep.count, 4);
}

static void test_move_dependency(void)
{
    VramReadDependency dep;
    int ok;

    memset(&dep, 0, sizeof(dep));
    ok = BuildMoveSourceDependency(100, 20, 5, 7, 1024, 512, &dep);
    expect_bool(ok, 1, "move normal");
    expect_count((int)dep.count, 1);

    memset(&dep, 0, sizeof(dep));
    ok = BuildMoveSourceDependency(1022, 510, 4, 4, 1024, 512, &dep);
    expect_bool(ok, 1, "move xy wrap");
    expect_count((int)dep.count, 4);
    if (dep.count >= 1)
        expect_rect(&dep.rect[0], 1022, 510, 1024, 512);
    if (dep.count >= 2)
        expect_rect(&dep.rect[1], 0, 510, 2, 512);
    if (dep.count >= 3)
        expect_rect(&dep.rect[2], 1022, 0, 1024, 2);
    if (dep.count >= 4)
        expect_rect(&dep.rect[3], 0, 0, 2, 2);

    memset(&dep, 0, sizeof(dep));
    ok = BuildMoveSourceDependency(100, 511, 8, 2, 1024, 512, &dep);
    expect_bool(ok, 1, "move y wrap");
    expect_count((int)dep.count, 2);
    if (dep.count >= 1)
        expect_rect(&dep.rect[0], 100, 511, 108, 512);
    if (dep.count >= 2)
        expect_rect(&dep.rect[1], 100, 0, 108, 1);
}

static void test_freshness(void)
{
    VramTileFreshnessInput in;

    memset(&in, 0, sizeof(in));
    in.efbCoverFull = 1;
    in.hasValidSnapshot = 1;

    in.cpuWriteEpoch = 10;
    in.materializedColorEpoch = 0;
    in.efbSeq = 9;
    expect_count((int)EvaluateVramTileFreshness(&in),
                 (int)VRAM_FRESH_NO_ACTION);

    in.efbSeq = 11;
    expect_count((int)EvaluateVramTileFreshness(&in),
                 (int)VRAM_FRESH_MATERIALIZED);

    in.materializedColorEpoch = 11;
    expect_count((int)EvaluateVramTileFreshness(&in),
                 (int)VRAM_FRESH_NO_ACTION);

    in.cpuWriteEpoch = 12;
    in.materializedColorEpoch = 11;
    in.efbSeq = 11;
    expect_count((int)EvaluateVramTileFreshness(&in),
                 (int)VRAM_FRESH_NO_ACTION);

    in.cpuWriteEpoch = 10;
    in.materializedColorEpoch = 0;
    in.efbSeq = 13;
    in.efbCoverFull = 0;
    expect_count((int)EvaluateVramTileFreshness(&in),
                 (int)VRAM_FRESH_UNRESOLVED);

    in.efbCoverFull = 1;
    in.rgb24 = 1;
    expect_count((int)EvaluateVramTileFreshness(&in),
                 (int)VRAM_FRESH_UNRESOLVED);

    in.rgb24 = 0;
    in.contaminated = 1;
    expect_count((int)EvaluateVramTileFreshness(&in),
                 (int)VRAM_FRESH_UNRESOLVED);

    in.contaminated = 0;
    in.mixedMapping = 1;
    expect_count((int)EvaluateVramTileFreshness(&in),
                 (int)VRAM_FRESH_UNRESOLVED);

    in.mixedMapping = 0;
    in.untrackedEfb = 1;
    expect_count((int)EvaluateVramTileFreshness(&in),
                 (int)VRAM_FRESH_UNRESOLVED);

    in.untrackedEfb = 0;
    in.hasValidSnapshot = 0;
    expect_count((int)EvaluateVramTileFreshness(&in),
                 (int)VRAM_FRESH_UNRESOLVED);

    in.hasValidSnapshot = 1;
    in.efbSeq = 0;
    expect_count((int)EvaluateVramTileFreshness(&in),
                 (int)VRAM_FRESH_NO_ACTION);
}

static void test_cpu_newer_probe_qualification(void)
{
    VramMoveShadowDecision decision;

    T6VramMoveShadowDecisionInit(&decision);
    decision.result = VRAM_FRESH_NO_ACTION;
    decision.unresolvedReason = T6_REASON_NONE;
    expect_bool(T6CpuNewerProbeQualifies(
                    100, 100, 80, 90, 95, &decision),
                1, "CPU-newer negative probe qualifies");
    expect_bool(T6CpuNewerProbeQualifies(
                    100, 99, 80, 90, 95, &decision),
                0, "probe requires exact A0 provenance sequence");
    expect_bool(T6CpuNewerProbeQualifies(
                    100, 100, 80, 100, 95, &decision),
                0, "CPU must be strictly newer than physical EFB");
    expect_bool(T6CpuNewerProbeQualifies(
                    100, 100, 80, 90, 100, &decision),
                0, "CPU must be strictly newer than snapshot");
    expect_bool(T6CpuNewerProbeQualifies(
                    100, 100, 100, 90, 95, &decision),
                0, "probe isolates CPU from materialized epoch");
    expect_bool(T6CpuNewerProbeQualifies(
                    100, 100, 80, 0, 0, &decision),
                0, "probe requires a real older EFB or snapshot source");
    expect_bool(T6CpuNewerProbeQualifies(
                    100, 100, 80, 90, 95, NULL),
                0, "CPU-newer probe rejects NULL decision");

    decision.result = VRAM_FRESH_MATERIALIZED;
    expect_bool(T6CpuNewerProbeQualifies(
                    100, 100, 80, 90, 95, &decision),
                0, "CPU-newer probe rejects materialized result");
    decision.result = VRAM_FRESH_NO_ACTION;
    decision.hazardTiles = 1;
    expect_bool(T6CpuNewerProbeQualifies(
                    100, 100, 80, 90, 95, &decision),
                0, "CPU-newer probe rejects a hazard");
    decision.hazardTiles = 0;
    decision.captureRequired = 1;
    expect_bool(T6CpuNewerProbeQualifies(
                    100, 100, 80, 90, 95, &decision),
                0, "CPU-newer probe rejects capture");
    decision.captureRequired = 0;
    decision.wouldCapture = 1;
    expect_bool(T6CpuNewerProbeQualifies(
                    100, 100, 80, 90, 95, &decision),
                0, "CPU-newer probe rejects capture plan");
    decision.wouldCapture = 0;
    decision.capturePlanSlot = 0;
    expect_bool(T6CpuNewerProbeQualifies(
                    100, 100, 80, 90, 95, &decision),
                0, "CPU-newer probe requires default capture slot");
    decision.capturePlanSlot = -1;
    decision.captureBufferReady = 1;
    expect_bool(T6CpuNewerProbeQualifies(
                    100, 100, 80, 90, 95, &decision),
                0, "CPU-newer probe rejects capture buffer");
    decision.captureBufferReady = 0;
    decision.requiredTiles = 1;
    expect_bool(T6CpuNewerProbeQualifies(
                    100, 100, 80, 90, 95, &decision),
                0, "CPU-newer probe rejects required tiles");
    decision.requiredTiles = 0;
    decision.requiredSeq = 1;
    expect_bool(T6CpuNewerProbeQualifies(
                    100, 100, 80, 90, 95, &decision),
                0, "CPU-newer probe rejects stale required sequence");
    decision.requiredSeq = 0;
    decision.requiredMapId = 1;
    expect_bool(T6CpuNewerProbeQualifies(
                    100, 100, 80, 90, 95, &decision),
                0, "CPU-newer probe rejects stale required map");
    decision.requiredMapId = 0;
    decision.unresolvedReason = T6_REASON_CAPTURE_FAIL;
    expect_bool(T6CpuNewerProbeQualifies(
                    100, 100, 80, 90, 95, &decision),
                0, "CPU-newer probe rejects unresolved reason");
}

typedef struct TileCountRecord
{
    int calls;
    int sawFirst;
    int sawLast;
} TileCountRecord;

static void record_tile(void *user, int tx, int ty)
{
    TileCountRecord *rec = (TileCountRecord *)user;

    rec->calls++;
    if (tx == 0 && ty == 0)
        rec->sawFirst = 1;
    if (tx == 63 && ty == 0)
        rec->sawLast = 1;
}

static void test_tile_enumeration(void)
{
    VramReadDependency dep;
    TileCountRecord rec;
    VramRect r;
    int n;

    memset(&dep, 0, sizeof(dep));
    r = (VramRect){ 0, 0, 16, 16 };
    VramReadDependencyAppend(&dep, &r);
    memset(&rec, 0, sizeof(rec));
    n = ForEachVramReadDependencyTile(&dep, 1024, 512, record_tile, &rec);
    expect_count(n, 1);
    expect_bool(rec.sawFirst, 1, "tile 0 enumerated");

    memset(&dep, 0, sizeof(dep));
    r = (VramRect){ 1008, 0, 1024, 16 };
    VramReadDependencyAppend(&dep, &r);
    memset(&rec, 0, sizeof(rec));
    n = ForEachVramReadDependencyTile(&dep, 1024, 512, record_tile, &rec);
    expect_count(n, 1);
    expect_bool(rec.sawLast, 1, "tile 63 enumerated");

    memset(&dep, 0, sizeof(dep));
    r = (VramRect){ 0, 0, 1024, 16 };
    VramReadDependencyAppend(&dep, &r);
    memset(&rec, 0, sizeof(rec));
    n = ForEachVramReadDependencyTile(&dep, 1024, 512, record_tile, &rec);
    expect_count(n, 64);
    expect_bool(rec.sawFirst, 1, "full row first tile");
    expect_bool(rec.sawLast, 1, "full row last tile");

    memset(&dep, 0, sizeof(dep));
    r = (VramRect){ 0, 0, 16, 16 };
    VramReadDependencyAppend(&dep, &r);
    VramReadDependencyAppend(&dep, &r);
    memset(&rec, 0, sizeof(rec));
    n = ForEachVramReadDependencyTile(&dep, 1024, 512, record_tile, &rec);
    expect_count(n, 2);

    /* Invalid dependency contract: empty, tampered count, out of range. */
    memset(&dep, 0, sizeof(dep));
    memset(&rec, 0, sizeof(rec));
    n = ForEachVramReadDependencyTile(&dep, 1024, 512, record_tile, &rec);
    expect_count(n, 0);
    expect_count(rec.calls, 0);

    memset(&dep, 0, sizeof(dep));
    r = (VramRect){ 0, 0, 16, 16 };
    VramReadDependencyAppend(&dep, &r);
    dep.count = T6_DEP_RECT_CAPACITY + 1;
    memset(&rec, 0, sizeof(rec));
    n = ForEachVramReadDependencyTile(&dep, 1024, 512, record_tile, &rec);
    expect_count(n, 0);
    expect_count(rec.calls, 0);

    memset(&dep, 0, sizeof(dep));
    dep.rect[0] = (VramRect){ 0, 0, 0, 0 };
    dep.count = 1;
    memset(&rec, 0, sizeof(rec));
    n = ForEachVramReadDependencyTile(&dep, 1024, 512, record_tile, &rec);
    expect_count(n, 0);
    expect_count(rec.calls, 0);

    memset(&dep, 0, sizeof(dep));
    r = (VramRect){ 1000, 0, 1100, 16 };
    VramReadDependencyAppend(&dep, &r);
    memset(&rec, 0, sizeof(rec));
    n = ForEachVramReadDependencyTile(&dep, 1024, 512, record_tile, &rec);
    expect_count(n, 0);
    expect_count(rec.calls, 0);

    memset(&dep, 0, sizeof(dep));
    r = (VramRect){ 0, 0, 16, 16 };
    VramReadDependencyAppend(&dep, &r);
    memset(&rec, 0, sizeof(rec));
    n = ForEachVramReadDependencyTile(&dep, 640, 480, record_tile, &rec);
    expect_count(n, 0);
    expect_count(rec.calls, 0);
}

static void test_interleaved_swizzle_coverage(void)
{
    int pages[2] = {7, 15};
    int modes[2] = {0, 1};
    int pi, mi, u, v, ok;

    for (pi = 0; pi < 2; pi++)
        for (mi = 0; mi < 2; mi++)
        {
            int pageid = pages[pi];
            int mode = modes[mi];
            VramReadDependency dep;
            int baseX = (pageid & 15) << 6;
            int baseY = (pageid >> 4) << 8;

            memset(&dep, 0, sizeof(dep));
            expect_bool(BuildStandardTextureDependency(
                            pageid, mode, 0, 0x1FF, 1024, 512,
                            0, 255, 0, 255, 1, &dep), 1,
                        "interleaved build");
            expect_count((int)dep.count,
                         (pageid == 15 && mode == 1) ? 6 : 4);

            ok = 1;
            for (v = 0; v < 256 && ok; v++)
                for (u = 0; u < 256; u++)
                {
                    int n_xi, n_yi, addrX, addrY, effX, effY;

                    if (mode == 0)
                    {
                        n_xi = ((u >> 2) & ~0x3c) + ((v << 2) & 0x3c);
                        n_yi = (v & ~0xf) + ((u >> 4) & 0xf);
                    }
                    else
                    {
                        n_xi = ((u >> 1) & ~0x78) + ((u << 2) & 0x40) +
                               ((v << 3) & 0x38);
                        n_yi = (v & ~0x7) + ((u >> 5) & 0x7);
                    }

                    addrX = baseX + n_xi;
                    addrY = baseY + n_yi;
                    /* The loader pointer arithmetic spills into the next row. */
                    effX = addrX & 1023;
                    effY = addrY + (addrX >> 10);
                    if (!dependency_contains_word(&dep, effX, effY))
                    {
                        ok = 0;
                        break;
                    }
                }
            expect_bool(ok, 1, "interleaved swizzle inside dependency");
        }
}

typedef struct MixedRecord
{
    int calls;
    int currentHazards;
    int previousNoAction;
    int cpuNewerAllNoAction;
} MixedRecord;

static int tile_is_current(int tx)
{
    return tx < 16;
}

static int tile_is_previous(int tx)
{
    return tx >= 32 && tx < 48;
}

static void record_mixed(void *user, int tx, int ty)
{
    MixedRecord *rec = (MixedRecord *)user;
    VramTileFreshnessInput in;

    (void)ty;
    rec->calls++;

    memset(&in, 0, sizeof(in));
    in.efbCoverFull = 1;
    in.hasValidSnapshot = 1;

    if (tile_is_current(tx))
    {
        in.cpuWriteEpoch = 10;
        in.efbSeq = 11;
        if (EvaluateVramTileFreshness(&in) == VRAM_FRESH_MATERIALIZED)
            rec->currentHazards++;
    }
    else if (tile_is_previous(tx))
    {
        in.cpuWriteEpoch = 10;
        in.efbSeq = 9;
        if (EvaluateVramTileFreshness(&in) == VRAM_FRESH_NO_ACTION)
            rec->previousNoAction++;
    }

    in.cpuWriteEpoch = 12;
    in.efbSeq = 11;
    if (EvaluateVramTileFreshness(&in) == VRAM_FRESH_NO_ACTION)
        rec->cpuNewerAllNoAction++;
}

static void test_mixed_dependency(void)
{
    VramReadDependency dep;
    MixedRecord rec;
    VramRect r;

    memset(&dep, 0, sizeof(dep));
    r = (VramRect){ 0, 0, 256, 256 };
    VramReadDependencyAppend(&dep, &r);
    r = (VramRect){ 512, 0, 768, 256 };
    VramReadDependencyAppend(&dep, &r);

    memset(&rec, 0, sizeof(rec));
    ForEachVramReadDependencyTile(&dep, 1024, 512, record_mixed, &rec);

    expect_count(rec.calls, 512);
    expect_count(rec.currentHazards, 256);
    expect_count(rec.previousNoAction, 256);
    expect_count(rec.cpuNewerAllNoAction, 512);
}

static void test_clut_boundary(void)
{
    VramReadDependency dep;
    int ok;

    /* 4-bit CLUT at x=1008 covers exactly [1008,1024). */
    memset(&dep, 0, sizeof(dep));
    ok = BuildWindowTextureDependency(0, 0, 0x3F, 0x1FF, 1024, 512, &dep);
    expect_bool(ok, 1, "clut4 build");
    expect_bool(dependency_contains_word(&dep, 1008, 0), 1,
                "clut4 last word");
    expect_bool(dependency_contains_word(&dep, 1023, 0), 1,
                "clut4 last word tail");
    expect_bool(dependency_contains_word(&dep, 1007, 0), 0,
                "clut4 before range");

    /* 8-bit CLUT at x=1008 wraps row-local and spills to row 1 linearly. */
    memset(&dep, 0, sizeof(dep));
    ok = BuildWindowTextureDependency(0, 1, 0x3F, 0x1FF, 1024, 512, &dep);
    expect_bool(ok, 1, "clut8 build");
    expect_bool(dependency_contains_word(&dep, 1008, 0), 1,
                "clut8 tail");
    expect_bool(dependency_contains_word(&dep, 1023, 0), 1,
                "clut8 tail last word");
    expect_bool(dependency_contains_word(&dep, 239, 0), 1,
                "clut8 row-local wrap");
    expect_bool(dependency_contains_word(&dep, 239, 1), 1,
                "clut8 linear spill");
    expect_bool(dependency_contains_word(&dep, 240, 0), 0,
                "clut8 outside row-local");
    expect_bool(dependency_contains_word(&dep, 240, 1), 0,
                "clut8 outside linear");
}

static void test_builder_invalid(void)
{
    VramReadDependency dep;

    memset(&dep, 0, sizeof(dep));
    expect_bool(BuildWindowTextureDependency(0, 3, 0, 0x1FF,
                                             1024, 512, &dep), 0,
                "window mode3 rejected");
    expect_count((int)dep.count, 0);

    memset(&dep, 0, sizeof(dep));
    expect_bool(BuildMoveSourceDependency(-1, 0, 8, 8, 1024, 512, &dep), 0,
                "move negative rejected");
    expect_count((int)dep.count, 0);

    memset(&dep, 0, sizeof(dep));
    expect_bool(BuildStandardTextureDependency(-1, 0, 0, 0x1FF, 1024, 512,
                                               0, 15, 0, 15, 0, &dep), 0,
                "standard negative page rejected");
    expect_count((int)dep.count, 0);

    memset(&dep, 0, sizeof(dep));
    expect_bool(BuildStandardTextureDependency(32, 0, 0, 0x1FF, 1024, 512,
                                               0, 15, 0, 15, 0, &dep), 0,
                "standard page32 rejected");
    expect_count((int)dep.count, 0);

    memset(&dep, 0, sizeof(dep));
    expect_bool(BuildStandardTextureDependency(0, 0, 0, 0x1FF, 1024, 512,
                                               5, 4, 0, 15, 0, &dep), 0,
                "standard invalid uv rejected");
    expect_count((int)dep.count, 0);

    memset(&dep, 0, sizeof(dep));
    expect_bool(BuildMoveSourceDependency(0, 512, 8, 8, 1024, 512, &dep), 0,
                "move y out of range rejected");
    expect_count((int)dep.count, 0);
}

static void test_capacity_injection(void)
{
    VramReadDependency dep;
    VramRect rects[12];
    int i;

    /* Synthetic 4+2+4+2=12 group: capacity 12 succeeds, 11 poisons. */
    for (i = 0; i < 12; i++)
        rects[i] = (VramRect){ i, 0, i + 1, 1 };

    memset(&dep, 0, sizeof(dep));
    expect_bool(VramReadDependencyAppendRects(&dep, rects, 12, 12), 1,
                "synthetic 12 append");
    expect_count((int)dep.count, 12);

    memset(&dep, 0, sizeof(dep));
    expect_bool(VramReadDependencyAppendRects(&dep, rects, 12, 11), 0,
                "synthetic 11 overflow");
    expect_count((int)dep.count, 0);

    /* Real 8-piece window dependency: capacity 8 succeeds, 7 poisons. */
    memset(&dep, 0, sizeof(dep));
    expect_bool(BuildWindowTextureDependencyWithCapacity(
                    15, 1, 0x3F, 0x1FF, 1024, 512, 8, &dep), 1,
                "window capacity 8");
    expect_count((int)dep.count, 8);

    memset(&dep, 0, sizeof(dep));
    expect_bool(BuildWindowTextureDependencyWithCapacity(
                    15, 1, 0x3F, 0x1FF, 1024, 512, 7, &dep), 0,
                "window capacity 7 overflow");
    expect_count((int)dep.count, 0);
    {
        int n = ForEachVramReadDependencyTile(&dep, 1024, 512,
                                              record_tile, NULL);
        expect_count(n, 0);
    }

    /* Capacity above the physical array must never write. */
    memset(&dep, 0, sizeof(dep));
    {
        VramRect r = { 0, 0, 16, 16 };
        expect_bool(VramReadDependencyAppendWithCapacity(
                        &dep, &r, T6_DEP_RECT_CAPACITY + 1), 0,
                    "capacity 13 rejected");
        expect_count((int)dep.count, 0);
        expect_bool(VramReadDependencyAppendWithCapacity(
                        &dep, &r, 0xFFFFFFFFu), 0,
                    "capacity UINT_MAX rejected");
        expect_count((int)dep.count, 0);
    }

    memset(&dep, 0, sizeof(dep));
    expect_bool(BuildWindowTextureDependencyWithCapacity(
                    15, 1, 0x3F, 0x1FF, 1024, 512,
                    T6_DEP_RECT_CAPACITY + 1, &dep), 0,
                "builder capacity 13 rejected");
    expect_count((int)dep.count, 0);
}

static void test_t6_materialize_decision(void)
{
    VramTileFreshnessInput in;

    memset(&in, 0, sizeof(in));
    in.efbCoverFull = 1;
    in.hasValidSnapshot = 1;
    in.cpuWriteEpoch = 10;
    in.materializedColorEpoch = 0;
    in.efbSeq = 11;
    in.snapshotSeq = 11;
    expect_count((int)EvaluateVramMaterializeTile(&in),
                 (int)VRAM_FRESH_MATERIALIZED);

    /* Repeated barrier for the same sequence is NO_ACTION. */
    in.materializedColorEpoch = 11;
    expect_count((int)EvaluateVramMaterializeTile(&in),
                 (int)VRAM_FRESH_NO_ACTION);

    memset(&in, 0, sizeof(in));
    in.efbCoverFull = 1;
    in.hasValidSnapshot = 1;
    in.cpuWriteEpoch = 12;
    in.efbSeq = 11;
    in.snapshotSeq = 11;
    expect_count((int)EvaluateVramMaterializeTile(&in),
                 (int)VRAM_FRESH_NO_ACTION);

    memset(&in, 0, sizeof(in));
    in.hasValidSnapshot = 1;
    in.cpuWriteEpoch = 10;
    in.efbSeq = 13;
    in.snapshotSeq = 13;
    in.efbCoverFull = 0;
    expect_count((int)EvaluateVramMaterializeTile(&in),
                 (int)VRAM_FRESH_UNRESOLVED);

    in.efbCoverFull = 1;
    in.rgb24 = 1;
    expect_count((int)EvaluateVramMaterializeTile(&in),
                 (int)VRAM_FRESH_UNRESOLVED);

    memset(&in, 0, sizeof(in));
    in.efbCoverFull = 1;
    in.hasValidSnapshot = 1;
    in.cpuWriteEpoch = 10;
    in.efbSeq = 13;
    in.snapshotSeq = 12;
    expect_count((int)EvaluateVramMaterializeTile(&in),
                 (int)VRAM_FRESH_UNRESOLVED);

    in.hasValidSnapshot = 0;
    in.snapshotSeq = 0;
    expect_count((int)EvaluateVramMaterializeTile(&in),
                 (int)VRAM_FRESH_UNRESOLVED);

    /* Stale baseline must not write; absent baseline keeps FULL fallback. */
    memset(&in, 0, sizeof(in));
    in.efbCoverFull = 1;
    in.hasValidSnapshot = 1;
    in.cpuWriteEpoch = 12;
    in.efbSeq = 13;
    in.snapshotSeq = 13;
    in.baselinePresentForSnapshot = 1;
    in.baselineUsable = 0;
    expect_count((int)EvaluateVramMaterializeTile(&in),
                 (int)VRAM_FRESH_UNRESOLVED);

    in.baselinePresentForSnapshot = 0;
    in.baselineUsable = 0;
    expect_count((int)EvaluateVramMaterializeTile(&in),
                 (int)VRAM_FRESH_MATERIALIZED);
}

static void test_t6_snapshot_selection(void)
{
    T6SnapshotCandidate c[2];
    uint64_t best = 0;

    memset(c, 0, sizeof(c));
    c[0].qualified = 1;
    c[0].seq = 11;
    c[0].sourcePriority = 30;
    c[0].captureOrder = 9;
    c[1].qualified = 1;
    c[1].seq = 13;
    c[1].sourcePriority = 10;
    c[1].captureOrder = 2;
    expect_count(T6SelectBestSnapshotTile(c, 2, &best), 2);
    expect_u64(best, 13, "newer snapshot wins");

    memset(c, 0, sizeof(c));
    c[0].qualified = 1;
    c[0].seq = 11;
    c[0].sourcePriority = 20;
    c[0].captureOrder = 9;
    c[1].qualified = 1;
    c[1].seq = 11;
    c[1].sourcePriority = 30;
    c[1].captureOrder = 2;
    expect_count(T6SelectBestSnapshotTile(c, 2, &best), 2);
    expect_u64(best, 11, "source priority tie-break");

    memset(c, 0, sizeof(c));
    c[0].qualified = 1;
    c[0].seq = 11;
    c[0].sourcePriority = 30;
    c[0].captureOrder = 5;
    c[1].qualified = 1;
    c[1].seq = 11;
    c[1].sourcePriority = 30;
    c[1].captureOrder = 3;
    expect_count(T6SelectBestSnapshotTile(c, 2, &best), 1);
    expect_u64(best, 11, "capture order tie-break");

    c[1].qualified = 0;
    expect_count(T6SelectBestSnapshotTile(c, 2, &best), 1);
    expect_u64(best, 11, "only one qualified");

    c[0].qualified = 0;
    expect_count(T6SelectBestSnapshotTile(c, 2, &best), 0);
}

static void test_t6_merge_pixel(void)
{
    expect_u64(T6MergePixelColor(0xFFFF, 0x1234), 0x9234,
               "mask bit preserved");
    expect_u64(T6MergePixelColor(0x7FFF, 0x8000u | 0x4321u), 0x4321,
               "RGB5A3 alpha never becomes mask");
    expect_u64(T6MergePixelColor(0x0000, 0x7ABC), 0x7ABC,
               "plain color write");
}

static unsigned int scalar_rgb5a3_offset(
    int px, int py, int vramX, int vramY, int vramW, int vramH,
    int viewportX, int viewportY, int viewportW, int viewportH,
    int texWidth)
{
    int nx = (int)((unsigned int)(px - vramX) & 1023u);
    int ny = (int)((unsigned int)(py - vramY) & 511u);
    int sx = viewportX + (nx * viewportW) / vramW;
    int sy = viewportY + (ny * viewportH) / vramH;

    return (unsigned int)((sy >> 2) * (texWidth >> 2) + (sx >> 2)) * 32u +
           (unsigned int)(((sy & 3) << 2) + (sx & 3)) * 2u;
}

static void test_rgb5a3_tile_sample_plan(void)
{
    static const int cases[][10] = {
        /* vram x/y/w/h, viewport x/y/w/h, texture w/h */
        {0, 0, 320, 240, 0, 0, 320, 240, 320, 240},
        {0, 256, 320, 240, 0, 0, 640, 480, 640, 480},
        {960, 0, 64, 512, 4, 8, 128, 464, 144, 480},
        {1008, 496, 32, 32, 0, 0, 64, 64, 64, 64}
    };
    unsigned char pixels[64];
    int c, tx, ty, lx, ly;

    memset(pixels, 0, sizeof(pixels));
    pixels[17] = 0xAB;
    pixels[18] = 0xCD;

    for (c = 0; c < (int)(sizeof(cases) / sizeof(cases[0])); c++)
    {
        const int *v = cases[c];

        for (ty = 0; ty < T6_VRAM_TILE_Y; ty++)
            for (tx = 0; tx < T6_VRAM_TILE_X; tx++)
            {
                T6RGB5A3TileSamplePlan plan;
                int inside = 1;

                for (ly = 0; ly < T6_VRAM_TILE_SIZE; ly++)
                    for (lx = 0; lx < T6_VRAM_TILE_SIZE; lx++)
                    {
                        int px = (tx << 4) + lx;
                        int py = (ty << 4) + ly;
                        int nx = (int)((unsigned int)(px - v[0]) & 1023u);
                        int ny = (int)((unsigned int)(py - v[1]) & 511u);

                        if (nx >= v[2] || ny >= v[3])
                            inside = 0;
                    }

                expect_bool(T6BuildRGB5A3TileSamplePlan(
                                &plan, tx, ty,
                                v[0], v[1], v[2], v[3],
                                v[4], v[5], v[6], v[7], v[8], v[9]),
                            inside, "tile sample plan coverage");
                if (!inside)
                    continue;

                for (ly = 0; ly < T6_VRAM_TILE_SIZE; ly++)
                    for (lx = 0; lx < T6_VRAM_TILE_SIZE; lx++)
                    {
                        unsigned int planned = plan.xByteOffset[lx] +
                                               plan.yByteOffset[ly];
                        unsigned int scalar = scalar_rgb5a3_offset(
                            (tx << 4) + lx, (ty << 4) + ly,
                            v[0], v[1], v[2], v[3],
                            v[4], v[5], v[6], v[7], v[8]);

                        if (planned != scalar)
                        {
                            printf("FAIL tile sample offset case=%d "
                                   "tile=%d,%d local=%d,%d got=%u exp=%u\n",
                                   c, tx, ty, lx, ly, planned, scalar);
                            s_failures++;
                            return;
                        }
                    }
            }
    }

    expect_bool(T6BuildRGB5A3TileSamplePlan(
                    NULL, 0, 0, 0, 0, 320, 240,
                    0, 0, 320, 240, 320, 240),
                0, "tile sample NULL plan rejected");
    {
        T6RGB5A3TileSamplePlan plan;

        expect_bool(T6BuildRGB5A3TileSamplePlan(
                        &plan, 0, 0, 0, 0, 0, 240,
                        0, 0, 320, 240, 320, 240),
                    0, "tile sample zero map rejected");
        memset(&plan, 0, sizeof(plan));
        plan.xByteOffset[0] = 17;
        expect_count((int)T6ReadRGB5A3TileSample(pixels, &plan, 0, 0),
                     0xABCD);
    }
}

static uint64_t simulate_c0_tile(uint64_t materialized,
                                 uint64_t snapshotSeq,
                                 int fullRect, int *changed)
{
    uint16_t old[16][16];
    unsigned int pixelsWritten = 0;
    int y, x, diff = 0;

    for (y = 0; y < 16; y++)
        for (x = 0; x < 16; x++)
            old[y][x] = 0x9000;

    for (y = 0; y < (fullRect ? 16 : 8); y++)
        for (x = 0; x < 16; x++)
        {
            uint16_t psx;

            if (!T6C0ColorCommitAllowed(snapshotSeq, materialized))
                continue;
            psx = T6MergePixelColor(old[y][x], 0x4321);
            if (psx != old[y][x])
                diff++;
            old[y][x] = psx;
            pixelsWritten++;
        }

    *changed = diff;
    return T6C0FullTilePromotionEpoch((int)pixelsWritten, 1,
                                      snapshotSeq, materialized);
}

static void test_t6_c0_cross(void)
{
    int changed;
    uint64_t epoch;

    expect_bool(T6C0ColorCommitAllowed(11, 12), 0,
                "older C0 snapshot blocked");
    expect_bool(T6C0ColorCommitAllowed(13, 12), 1,
                "newer C0 snapshot allowed");
    expect_bool(T6C0ColorCommitAllowed(12, 12), 1,
                "equal sequence allowed");

    epoch = simulate_c0_tile(12, 11, 1, &changed);
    expect_count(changed, 0);
    expect_u64(epoch, 12, "blocked C0 keeps epoch");

    epoch = simulate_c0_tile(12, 13, 1, &changed);
    expect_count(changed, 256);
    expect_u64(epoch, 13, "full C0 tile promotes");

    epoch = simulate_c0_tile(12, 13, 0, &changed);
    expect_count(changed, 128);
    expect_u64(epoch, 12, "partial C0 never promotes whole tile");
    expect_u64(T6C0FullTilePromotionEpoch(65536 + 256, 1, 13, 12),
               12, "oversized C0 count must not wrap to full tile");
}

static void test_filtered_resolution_eligibility(void)
{
    unsigned int resolved = 0;
    int i;

    expect_bool(T6FilteredResolutionEligible(1, 1, 1, 12, 12, 1),
                1, "current full readable baseline resolves pixel");
    expect_bool(T6FilteredResolutionEligible(0, 1, 1, 12, 12, 1),
                0, "wrong required snapshot cannot resolve");
    expect_bool(T6FilteredResolutionEligible(1, 0, 1, 12, 12, 1),
                0, "sampling-map mismatch cannot resolve");
    expect_bool(T6FilteredResolutionEligible(1, 1, 0, 12, 12, 1),
                0, "partial baseline tile cannot resolve");
    expect_bool(T6FilteredResolutionEligible(1, 1, 1, 0, 0, 1),
                0, "missing baseline sequence cannot resolve");
    expect_bool(T6FilteredResolutionEligible(1, 1, 1, 11, 12, 1),
                0, "baseline older than CPU write cannot resolve");
    expect_bool(T6FilteredResolutionEligible(1, 1, 1, 12, 12, 0),
                0, "raw pixel read failure cannot resolve");

    for (i = 0; i < 256; i++)
        resolved += (unsigned int)T6FilteredResolutionEligible(
            1, 1, 1, 12, 12, i != 255);
    expect_count((int)resolved, 255);
    expect_u64(T6FullTileResolutionPromotionEpoch(resolved, 1, 13, 12),
               12, "one unreadable pixel blocks whole-tile promotion");
}

typedef struct T6RunCount
{
    int runs;
} T6RunCount;

static void count_t6_run(void *user, int x0, int y0, int x1, int y1)
{
    T6RunCount *rc = (T6RunCount *)user;

    (void)x0;
    (void)y0;
    (void)x1;
    (void)y1;
    rc->runs++;
}

static void test_t6_plan(void)
{
    VramMaterializePlan plan;
    T6RunCount rc;

    T6MaterializePlanReset(&plan);
    expect_count(plan.hazardCount, 0);
    expect_count(plan.resolvedCount, 0);
    expect_count(plan.changedCount, 0);
    expect_bool(T6MaterializePlanAllResolved(&plan), 1, "empty plan resolved");

    T6MaterializePlanMarkHazard(&plan, 1, 0);
    T6MaterializePlanMarkHazard(&plan, 2, 0);
    T6MaterializePlanMarkHazard(&plan, 1, 1);
    expect_count(plan.hazardCount, 3);
    expect_bool(T6MaterializePlanAllResolved(&plan), 0, "unresolved batch");

    T6MaterializePlanMarkResolved(&plan, 1, 0);
    T6MaterializePlanMarkResolved(&plan, 2, 0);
    expect_bool(T6MaterializePlanAllResolved(&plan), 0, "still unresolved");
    T6MaterializePlanMarkResolved(&plan, 1, 1);
    expect_bool(T6MaterializePlanAllResolved(&plan), 1, "all resolved");

    T6MaterializePlanMarkChanged(&plan, 1, 0);
    T6MaterializePlanMarkChanged(&plan, 2, 0);
    T6MaterializePlanMarkChanged(&plan, 1, 1);
    T6MaterializePlanMarkChanged(&plan, 1, 0);
    expect_count(plan.changedCount, 3);

    memset(&rc, 0, sizeof(rc));
    expect_count(ForEachHorizontalTileRun(
                     &plan.changed[0][0], T6_VRAM_TILE_X, T6_VRAM_TILE_Y,
                     T6_VRAM_TILE_SIZE, count_t6_run, &rc),
                 2);
    expect_count(rc.runs, 2);
}

typedef struct T6Harness
{
    uint16_t vram[1024 * 512];
    uint64_t cpuEpoch[T6_VRAM_TILE_Y][T6_VRAM_TILE_X];
    uint64_t matEpoch[T6_VRAM_TILE_Y][T6_VRAM_TILE_X];
    uint64_t snapshotSeq[T6_VRAM_TILE_Y][T6_VRAM_TILE_X];
    int snapshotPresent[T6_VRAM_TILE_Y][T6_VRAM_TILE_X];
    int snapshotFull[T6_VRAM_TILE_Y][T6_VRAM_TILE_X];
    int snapshotSlot[T6_VRAM_TILE_Y][T6_VRAM_TILE_X];
    int physPresent[T6_VRAM_TILE_Y][T6_VRAM_TILE_X];
    uint64_t physSeq[T6_VRAM_TILE_Y][T6_VRAM_TILE_X];
    int baselinePresent[T6_VRAM_TILE_Y][T6_VRAM_TILE_X];
    int baselineUsable[T6_VRAM_TILE_Y][T6_VRAM_TILE_X];
    int mapKind[T6_VRAM_TILE_Y][T6_VRAM_TILE_X];
    uint32_t tileMapId[T6_VRAM_TILE_Y][T6_VRAM_TILE_X];
    int rgb24;
    int contaminated;
    int mixedMapping;
    int untrackedEfb;
    int captureSucceed;
    int captureBufferReady;
    int captureFull;
    int overwriteRequiredSlot;
    int captures;
    int writes;
    int invalidateRuns;
    int touchedCount;
    int overflow;
    int validateIterations;
    int writeIterations;
    int commitIterations;
    int wndHits;
    int wndUploads;
    unsigned int wndKey[8];
    int wndUsed[8];
    int wndPage[8];
    int wndMode[8];
    unsigned int wndClut[8];
    int wndCount;
    unsigned int stdKey[8];
    int stdUsed[8];
    int stdPage[8];
    int stdMode[8];
    unsigned int stdClut[8];
    int stdU0[8];
    int stdU1[8];
    int stdV0[8];
    int stdV1[8];
    int stdIL[8];
    int stdCount;
    int stdHits;
    int stdUploads;
    unsigned char changed[T6_VRAM_TILE_Y][T6_VRAM_TILE_X];
    uint32_t mapId;
} T6Harness;

static T6Harness s_harness;
static VramMaterializeWorkspace s_ws;

static void harness_reset(T6Harness *h)
{
    int ty, tx;

    memset(h, 0, sizeof(*h));
    for (ty = 0; ty < T6_VRAM_TILE_Y; ty++)
        for (tx = 0; tx < T6_VRAM_TILE_X; tx++)
        {
            h->snapshotSlot[ty][tx] = -1;
            h->mapKind[ty][tx] = T6_MAP_CURRENT;
            h->tileMapId[ty][tx] = 1;
        }
    for (tx = 0; tx < 1024 * 512; tx++)
        h->vram[tx] = 0x8000;
    h->captureSucceed = 1;
    h->captureBufferReady = 1;
    h->captureFull = 1;
    h->mapId = 1;
}

static uint16_t harness_color(int tx, int ty, int px, int py)
{
    return (uint16_t)(0x1000 +
                      ((tx * 7 + ty * 3 + px + py * 5) & 0x0FFF));
}

/* Pre-fill one tile with exactly what harness_write() would materialize, so
 * a same-color barrier commits the epoch without changedCount. */
static void harness_prefill_tile(T6Harness *h, int tx, int ty)
{
    int px, py;

    for (py = ty << 4; py < (ty << 4) + T6_VRAM_TILE_SIZE; py++)
        for (px = tx << 4; px < (tx << 4) + T6_VRAM_TILE_SIZE; px++)
            h->vram[py * 1024 + px] =
                (uint16_t)(0x8000 |
                           (harness_color(tx, ty, px, py) & 0x7FFF));
}

typedef struct T6HarnessRun
{
    T6Harness *h;
    VramMaterializeSession *session;
    unsigned char seen[T6_VRAM_TILE_Y][T6_VRAM_TILE_X];
} T6HarnessRun;

static void harness_observe(void *user, int tx, int ty)
{
    T6HarnessRun *run = (T6HarnessRun *)user;
    T6Harness *h = run->h;
    uint64_t psxColor, snapSeq, efbSeq;
    int ttx = tx & 63;
    int tty = ty & 31;
    int slot = -1;

    if (run->seen[tty][ttx])
        return;
    run->seen[tty][ttx] = 1;

    snapSeq = h->snapshotPresent[tty][ttx] ?
              h->snapshotSeq[tty][ttx] : 0;
    psxColor = h->cpuEpoch[tty][ttx] > h->matEpoch[tty][ttx] ?
               h->cpuEpoch[tty][ttx] : h->matEpoch[tty][ttx];
    efbSeq = h->physPresent[tty][ttx] &&
             h->physSeq[tty][ttx] > snapSeq ?
             h->physSeq[tty][ttx] : snapSeq;

    if (h->snapshotPresent[tty][ttx] && snapSeq >= efbSeq)
        slot = h->snapshotSlot[tty][ttx];

    T6MaterializeSessionObserveTile(
        run->session, tx, ty, psxColor, efbSeq, snapSeq,
        h->tileMapId[tty][ttx], slot, h->mapKind[tty][ttx],
        h->physPresent[tty][ttx], h->physSeq[tty][ttx]);
}

static int harness_validate(void *user, int tx, int ty,
                            uint64_t requiredSeq,
                            VramTileFreshnessInput *in)
{
    T6HarnessRun *run = (T6HarnessRun *)user;
    T6Harness *h = run->h;
    int ttx = tx & 63;
    int tty = ty & 31;

    in->cpuWriteEpoch = h->cpuEpoch[tty][ttx];
    in->materializedColorEpoch = h->matEpoch[tty][ttx];
    in->snapshotSeq = h->snapshotPresent[tty][ttx] ?
                      h->snapshotSeq[tty][ttx] : 0;
    in->rgb24 = h->rgb24;
    in->contaminated = h->contaminated;
    in->mixedMapping = h->mixedMapping;
    in->untrackedEfb = h->untrackedEfb;
    if (h->mapKind[tty][ttx] == T6_MAP_UNKNOWN)
        in->untrackedEfb = 1;
    in->hasValidSnapshot =
        h->snapshotPresent[tty][ttx] &&
        h->snapshotFull[tty][ttx] &&
        in->snapshotSeq >= requiredSeq;
    in->baselinePresentForSnapshot = h->baselinePresent[tty][ttx];
    in->baselineUsable = h->baselineUsable[tty][ttx];
    in->efbCoverFull = h->snapshotFull[tty][ttx];
    return 1;
}

static void harness_write(void *user, VramMaterializePlan *plan,
                          int tx, int ty, uint64_t requiredSeq)
{
    T6HarnessRun *run = (T6HarnessRun *)user;
    T6Harness *h = run->h;
    int ttx = tx & 63;
    int tty = ty & 31;
    int px, py;
    int baselineDelta =
        h->baselinePresent[tty][ttx] && h->baselineUsable[tty][ttx];

    (void)requiredSeq;
    for (py = ty << 4; py < (ty << 4) + T6_VRAM_TILE_SIZE; py++)
        for (px = tx << 4; px < (tx << 4) + T6_VRAM_TILE_SIZE; px++)
        {
            uint16_t old = h->vram[py * 1024 + px];
            uint16_t color;
            uint16_t psx;

            if (baselineDelta && ((px & 3) != 0 || (py & 3) != 0))
                continue;

            color = harness_color(tx, ty, px, py);
            psx = T6MergePixelColor(old, color);
            if (psx == old)
                continue;

            h->vram[py * 1024 + px] = psx;
            h->writes++;
            T6MaterializePlanMarkChanged(plan, tx, ty);
        }
}

static uint64_t harness_seq(void *user, int tx, int ty)
{
    T6HarnessRun *run = (T6HarnessRun *)user;
    T6Harness *h = run->h;
    int ttx = tx & 63;
    int tty = ty & 31;

    return h->snapshotPresent[tty][ttx] ?
           h->snapshotSeq[tty][ttx] : 0;
}

static void harness_apply_epoch(void *user, int tx, int ty, uint64_t seq)
{
    T6HarnessRun *run = (T6HarnessRun *)user;
    T6Harness *h = run->h;
    int ttx = tx & 63;
    int tty = ty & 31;

    if (seq > h->matEpoch[tty][ttx])
        h->matEpoch[tty][ttx] = seq;
}

static void harness_invalidate(void *user, int x0, int y0,
                               int x1, int y1)
{
    T6Harness *h = (T6Harness *)user;

    (void)x0;
    (void)y0;
    (void)x1;
    (void)y1;
    h->invalidateRuns++;
}

static VramFreshResult harness_run(T6Harness *h,
                                   const VramReadDependency *dep)
{
    VramMaterializeSession session;
    T6HarnessRun run;
    int slot;
    int ty, tx;

    memset(&run, 0, sizeof(run));
    run.h = h;
    run.session = &session;
    memset(h->changed, 0, sizeof(h->changed));
    T6MaterializeSessionInit(&session, &s_ws);
    ForEachVramReadDependencyTile(dep, 1024, 512,
                                  harness_observe, &run);

    if (session.captureRequired)
    {
        if (session.captureConflict ||
            !T6MaterializeSessionSelectCaptureSlot(&session, 0, &slot))
            return VRAM_FRESH_UNRESOLVED;
        if (!h->captureSucceed)
            return VRAM_FRESH_UNRESOLVED;

        h->captures++;
        {
            int cx = session.captureTileX & 63;
            int cy = session.captureTileY & 31;

            h->snapshotPresent[cy][cx] = 1;
            h->snapshotFull[cy][cx] = 1;
            h->snapshotSeq[cy][cx] = h->physSeq[cy][cx];
            h->snapshotSlot[cy][cx] = slot;

            if (h->overwriteRequiredSlot)
            {
                int other = 1 - slot;

                for (ty = 0; ty < T6_VRAM_TILE_Y; ty++)
                    for (tx = 0; tx < T6_VRAM_TILE_X; tx++)
                        if (session.plan.hazard[ty][tx] &&
                            T6MaterializeSessionRequiredSlot(
                                &session, tx, ty) == other)
                            h->snapshotPresent[ty][tx] = 0;
            }
        }
    }

    if (session.plan.hazardCount == 0)
    {
        h->touchedCount = session.touchedCount;
        h->overflow = session.overflow;
        h->validateIterations = session.validateIterations;
        h->writeIterations = session.writeIterations;
        h->commitIterations = session.commitIterations;
        return VRAM_FRESH_NO_ACTION;
    }

    if (!T6MaterializeSessionValidate(&session,
                                      harness_validate, &run))
        return VRAM_FRESH_UNRESOLVED;

    T6MaterializeSessionWrite(&session, harness_write, &run);
    T6MaterializeSessionCommitEpoch(&session, harness_seq,
                                    harness_apply_epoch, &run);

    if (session.plan.changedCount > 0)
        (void)T6MaterializeSessionInvalidateChanged(
            &session, harness_invalidate, h);

    h->touchedCount = session.touchedCount;
    h->overflow = session.overflow;
    h->validateIterations = session.validateIterations;
    h->writeIterations = session.writeIterations;
    h->commitIterations = session.commitIterations;
    memcpy(h->changed, session.plan.changed, sizeof(h->changed));

    return VRAM_FRESH_MATERIALIZED;
}

static uint32_t tile_hash(const T6Harness *h, int tx, int ty)
{
    uint32_t hash = 2166136261u;
    int px, py;

    for (py = ty << 4; py < (ty << 4) + T6_VRAM_TILE_SIZE; py++)
        for (px = tx << 4; px < (tx << 4) + T6_VRAM_TILE_SIZE; px++)
            hash = (hash ^ h->vram[py * 1024 + px]) * 16777619u;
    return hash;
}

static void append_tile_dep(VramReadDependency *dep, int tx, int ty)
{
    VramRect r;

    r.x0 = tx << 4;
    r.y0 = ty << 4;
    r.x1 = r.x0 + T6_VRAM_TILE_SIZE;
    r.y1 = r.y0 + T6_VRAM_TILE_SIZE;
    VramReadDependencyAppend(dep, &r);
}

static unsigned int harness_le32(const T6Harness *h, int addr)
{
    return (unsigned int)h->vram[addr] |
           ((unsigned int)h->vram[addr + 1] << 16);
}

/* Exact production palette checksum: 32-bit grouped reads, row-weighted and
 * high-word folded, so the harness key matches LoadTextureWnd(). */
static unsigned int production_palette_checksum(const T6Harness *h,
                                                unsigned int clutId,
                                                int mode)
{
    int cx = ((int)(clutId & 0x3F) << 4);
    int cy = (int)((clutId >> 6) & 0x1FF);
    unsigned int l = 0;
    unsigned int words = mode == 1 ? 128u : 8u;
    unsigned int row;
    unsigned int addr = (unsigned int)(cy * 1024 + cx);

    for (row = 1; row <= words; row++)
    {
        unsigned int v = harness_le32(h, (int)(addr & 0x7FFFFu));

        if (mode == 1)
            l += (v - 1u) * row;
        else
            l += (v - 1u) << row;
        addr += 2;
    }
    l = (l + (l >> 16)) & 0x3FFFu;
    return l;
}

static void harness_set_tile(T6Harness *h, int tx, int ty, uint64_t seq)
{
    h->physPresent[ty][tx] = 1;
    h->physSeq[ty][tx] = seq;
    h->snapshotPresent[ty][tx] = 1;
    h->snapshotFull[ty][tx] = 1;
    h->snapshotSeq[ty][tx] = seq;
    h->snapshotSlot[ty][tx] = 0;
}

static void harness_set_phys_only(T6Harness *h, int tx, int ty,
                                  uint64_t seq)
{
    h->physPresent[ty][tx] = 1;
    h->physSeq[ty][tx] = seq;
    h->snapshotPresent[ty][tx] = 0;
}

static void mark_dep_tile(void *user, int tx, int ty)
{
    unsigned char (*tiles)[T6_VRAM_TILE_X] =
        (unsigned char (*)[T6_VRAM_TILE_X])user;

    tiles[ty & 31][tx & 63] = 1;
}

static int wnd_entry_depends_on_changed(const T6Harness *h,
                                        int pageid, int mode,
                                        unsigned int clutId)
{
    VramReadDependency dep;
    unsigned char tiles[T6_VRAM_TILE_Y][T6_VRAM_TILE_X];
    int ty, tx;

    memset(&dep, 0, sizeof(dep));
    memset(tiles, 0, sizeof(tiles));
    if (!BuildWindowTextureDependency(pageid, mode, clutId & 0x7FFF,
                                      0x1FF, 1024, 512, &dep))
        return 0;

    ForEachVramReadDependencyTile(&dep, 1024, 512,
                                  mark_dep_tile, tiles);
    for (ty = 0; ty < T6_VRAM_TILE_Y; ty++)
        for (tx = 0; tx < T6_VRAM_TILE_X; tx++)
            if (tiles[ty][tx] && h->changed[ty][tx])
                return 1;
    return 0;
}

static VramFreshResult simulate_window_lookup(T6Harness *h,
                                              int pageid, int mode,
                                              unsigned int clutId,
                                              int *hitOut)
{
    VramReadDependency dep;
    VramFreshResult result;
    unsigned int key;
    int i;

    memset(&dep, 0, sizeof(dep));
    if (!BuildWindowTextureDependency(pageid, mode, clutId & 0x7FFF,
                                      0x1FF, 1024, 512, &dep))
        return VRAM_FRESH_UNRESOLVED;

    result = harness_run(h, &dep);

    if (mode == 2)
        key = 0;
    else
        key = (clutId & 0x7FFF) |
              (((uint32_t)production_palette_checksum(
                  h, clutId, mode)) << 16);

    /* Materialize may have invalidated entries that depend on changed tiles. */
    if (result == VRAM_FRESH_MATERIALIZED)
    {
        for (i = 0; i < h->wndCount; i++)
            if (h->wndUsed[i] &&
                wnd_entry_depends_on_changed(
                    h, h->wndPage[i], h->wndMode[i], h->wndClut[i]))
                h->wndUsed[i] = 0;
    }

    *hitOut = 0;
    for (i = 0; i < h->wndCount; i++)
        if (h->wndUsed[i] && h->wndKey[i] == key)
        {
            h->wndHits++;
            *hitOut = 1;
            return result;
        }

    if (h->wndCount < 8)
    {
        h->wndUsed[h->wndCount] = 1;
        h->wndKey[h->wndCount] = key;
        h->wndPage[h->wndCount] = pageid;
        h->wndMode[h->wndCount] = mode;
        h->wndClut[h->wndCount] = clutId & 0x7FFF;
        h->wndCount++;
    }
    h->wndUploads++;
    return result;
}

static int standard_entry_depends_on_changed(const T6Harness *h,
                                             int pageid, int mode,
                                             unsigned int clutId,
                                             int uMin, int uMax,
                                             int vMin, int vMax,
                                             int interleaved)
{
    VramReadDependency dep;
    unsigned char tiles[T6_VRAM_TILE_Y][T6_VRAM_TILE_X];
    int ty, tx;

    memset(&dep, 0, sizeof(dep));
    memset(tiles, 0, sizeof(tiles));
    if (!BuildStandardTextureDependency(pageid, mode, clutId & 0x7FFF,
                                        0x1FF, 1024, 512,
                                        uMin, uMax, vMin, vMax,
                                        interleaved, &dep))
        return 0;

    ForEachVramReadDependencyTile(&dep, 1024, 512,
                                  mark_dep_tile, tiles);
    for (ty = 0; ty < T6_VRAM_TILE_Y; ty++)
        for (tx = 0; tx < T6_VRAM_TILE_X; tx++)
            if (tiles[ty][tx] && h->changed[ty][tx])
                return 1;
    return 0;
}

/* Production CheckTextureInSubSCache() compares page/mode/CLUT key and the
 * stored UV rect must cover the requested one (INCHECK semantics). */
static int std_entry_covers_lookup(const T6Harness *h, int entryIndex,
                                   int pageid, int mode,
                                   unsigned int key,
                                   int uMin, int uMax,
                                   int vMin, int vMax,
                                   int interleaved)
{
    /* Production CheckTextureInSubSCache() does not compare GlobalTextIL;
     * interleaved state only affects dependency construction, not identity. */
    (void)interleaved;
    if (entryIndex < 0 || entryIndex >= 8 || !h->stdUsed[entryIndex])
        return 0;
    return h->stdPage[entryIndex] == pageid &&
           h->stdMode[entryIndex] == mode &&
           h->stdKey[entryIndex] == key &&
           uMin >= h->stdU0[entryIndex] &&
           uMax <= h->stdU1[entryIndex] &&
           vMin >= h->stdV0[entryIndex] &&
           vMax <= h->stdV1[entryIndex];
}

/* Host callback for the shared full-page invalidation core.  It applies the
 * same packed-position XCHECK as production's SOFFA..SOFFD sweep. */
static void t6_harness_invalidate_standard_page(
    void *user, int pageid, int mode, int partition,
    unsigned int packedPos)
{
    T6Harness *h = (T6Harness *)user;
    int i;

    (void)partition;
    for (i = 0; i < h->stdCount; i++)
        if (h->stdUsed[i] &&
            h->stdPage[i] == pageid &&
            h->stdMode[i] == mode &&
            T6PackedPosIntersects(
                T6StandardEntryPackedPos(h->stdU0[i], h->stdU1[i],
                                         h->stdV0[i], h->stdV1[i]),
                packedPos))
            h->stdUsed[i] = 0;
}

static VramFreshResult simulate_standard_lookup(T6Harness *h,
                                                int pageid, int mode,
                                                unsigned int clutId,
                                                int uMin, int uMax,
                                                int vMin, int vMax,
                                                int interleaved,
                                                int *hitOut)
{
    VramReadDependency dep;
    VramFreshResult result;
    unsigned int key;
    unsigned int changedTiles = 0;
    int i;
    int ty, tx;

    memset(&dep, 0, sizeof(dep));
    if (!BuildStandardTextureDependency(pageid, mode, clutId & 0x7FFF,
                                        0x1FF, 1024, 512,
                                        uMin, uMax, vMin, vMax,
                                        interleaved, &dep))
        return VRAM_FRESH_UNRESOLVED;

    result = harness_run(h, &dep);
    for (ty = 0; ty < T6_VRAM_TILE_Y; ty++)
        for (tx = 0; tx < T6_VRAM_TILE_X; tx++)
            if (h->changed[ty][tx])
                changedTiles++;

    if (mode == 2)
        key = 0;
    else
        key = (clutId & 0x7FFF) |
              (((uint32_t)production_palette_checksum(
                  h, clutId, mode)) << 16);

    if (result == VRAM_FRESH_MATERIALIZED)
    {
        T6InterleavedStandardBarrier(
            result, changedTiles, interleaved,
            pageid, mode, t6_harness_invalidate_standard_page, h);
        if (!interleaved)
        {
            for (i = 0; i < h->stdCount; i++)
                if (h->stdUsed[i] &&
                    standard_entry_depends_on_changed(
                        h, h->stdPage[i], h->stdMode[i], h->stdClut[i],
                        h->stdU0[i], h->stdU1[i], h->stdV0[i], h->stdV1[i],
                        h->stdIL[i]))
                    h->stdUsed[i] = 0;
        }
    }

    *hitOut = 0;
    for (i = 0; i < h->stdCount; i++)
        if (std_entry_covers_lookup(
                h, i, pageid, mode, key,
                uMin, uMax, vMin, vMax, interleaved))
        {
            h->stdHits++;
            *hitOut = 1;
            return result;
        }

    if (h->stdCount < 8)
    {
        h->stdUsed[h->stdCount] = 1;
        h->stdKey[h->stdCount] = key;
        h->stdPage[h->stdCount] = pageid;
        h->stdMode[h->stdCount] = mode;
        h->stdClut[h->stdCount] = clutId & 0x7FFF;
        h->stdU0[h->stdCount] = uMin;
        h->stdU1[h->stdCount] = uMax;
        h->stdV0[h->stdCount] = vMin;
        h->stdV1[h->stdCount] = vMax;
        h->stdIL[h->stdCount] = interleaved;
        h->stdCount++;
    }
    h->stdUploads++;
    return result;
}

static void test_materialize_core_repeat(void)
{
    T6Harness *h = &s_harness;
    VramReadDependency dep;
    uint32_t before, after;

    harness_reset(h);
    memset(&dep, 0, sizeof(dep));
    append_tile_dep(&dep, 0, 0);

    h->cpuEpoch[0][0] = 10;
    h->physPresent[0][0] = 1;
    h->physSeq[0][0] = 11;
    h->snapshotPresent[0][0] = 1;
    h->snapshotFull[0][0] = 1;
    h->snapshotSeq[0][0] = 11;
    h->snapshotSlot[0][0] = 0;

    expect_count((int)harness_run(h, &dep),
                 (int)VRAM_FRESH_MATERIALIZED);
    expect_count(h->captures, 0);
    expect_count(h->writes, 256);
    expect_count(h->invalidateRuns, 1);
    expect_count(h->touchedCount, 1);
    expect_count(h->validateIterations, 1);
    expect_count(h->writeIterations, 1);
    expect_count(h->commitIterations, 1);
    expect_u64(h->matEpoch[0][0], 11, "core epoch committed");
    expect_u64((uint64_t)(h->vram[0] & 0x8000), 0x8000,
               "core preserves mask bit");

    before = tile_hash(h, 0, 0);
    expect_count((int)harness_run(h, &dep),
                 (int)VRAM_FRESH_NO_ACTION);
    expect_count(h->captures, 0);
    expect_count(h->writes, 256);
    after = tile_hash(h, 0, 0);
    expect_bool(before == after, 1,
                "second call leaves vram unchanged");
}

static void test_materialize_core_fail_no_change(void)
{
    T6Harness *h = &s_harness;
    VramReadDependency dep;
    uint32_t before;

    harness_reset(h);
    memset(&dep, 0, sizeof(dep));
    append_tile_dep(&dep, 0, 0);
    before = tile_hash(h, 0, 0);

    h->cpuEpoch[0][0] = 12;
    h->physPresent[0][0] = 1;
    h->physSeq[0][0] = 11;
    h->snapshotPresent[0][0] = 1;
    h->snapshotFull[0][0] = 1;
    h->snapshotSeq[0][0] = 11;
    h->snapshotSlot[0][0] = 0;
    expect_count((int)harness_run(h, &dep),
                 (int)VRAM_FRESH_NO_ACTION);
    expect_count(h->writes, 0);
    expect_count(h->invalidateRuns, 0);
    expect_u64(h->matEpoch[0][0], 0, "cpu newer no epoch");
    expect_bool(tile_hash(h, 0, 0) == before, 1,
                "cpu newer hash unchanged");

    harness_reset(h);
    h->snapshotPresent[0][0] = 1;
    h->snapshotFull[0][0] = 0;
    h->snapshotSeq[0][0] = 13;
    h->snapshotSlot[0][0] = 0;
    before = tile_hash(h, 0, 0);
    expect_count((int)harness_run(h, &dep),
                 (int)VRAM_FRESH_UNRESOLVED);
    expect_count(h->writes, 0);
    expect_u64(h->matEpoch[0][0], 0, "partial no epoch");
    expect_bool(tile_hash(h, 0, 0) == before, 1,
                "partial hash unchanged");

    harness_reset(h);
    h->rgb24 = 1;
    h->physPresent[0][0] = 1;
    h->physSeq[0][0] = 13;
    h->snapshotPresent[0][0] = 1;
    h->snapshotFull[0][0] = 1;
    h->snapshotSeq[0][0] = 13;
    h->snapshotSlot[0][0] = 0;
    before = tile_hash(h, 0, 0);
    expect_count((int)harness_run(h, &dep),
                 (int)VRAM_FRESH_UNRESOLVED);
    expect_count(h->writes, 0);
    expect_u64(h->matEpoch[0][0], 0, "rgb24 no epoch");
    expect_bool(tile_hash(h, 0, 0) == before, 1,
                "rgb24 hash unchanged");

    harness_reset(h);
    h->captureSucceed = 0;
    h->physPresent[0][0] = 1;
    h->physSeq[0][0] = 13;
    h->snapshotPresent[0][0] = 1;
    h->snapshotFull[0][0] = 1;
    h->snapshotSeq[0][0] = 11;
    h->snapshotSlot[0][0] = 0;
    before = tile_hash(h, 0, 0);
    expect_count((int)harness_run(h, &dep),
                 (int)VRAM_FRESH_UNRESOLVED);
    expect_count(h->captures, 0);
    expect_count(h->writes, 0);
    expect_u64(h->matEpoch[0][0], 0, "capture fail no epoch");
    expect_bool(tile_hash(h, 0, 0) == before, 1,
                "capture fail hash unchanged");
}

static void test_materialize_core_slot_replacement(void)
{
    T6Harness *h = &s_harness;
    VramReadDependency dep;

    harness_reset(h);
    memset(&dep, 0, sizeof(dep));
    append_tile_dep(&dep, 0, 0);
    append_tile_dep(&dep, 1, 0);

    h->mapKind[0][0] = T6_MAP_CURRENT;
    h->tileMapId[0][0] = 1;
    h->physPresent[0][0] = 1;
    h->physSeq[0][0] = 13;

    h->mapKind[0][1] = T6_MAP_PREVIOUS;
    h->tileMapId[0][1] = 2;
    h->snapshotPresent[0][1] = 1;
    h->snapshotFull[0][1] = 1;
    h->snapshotSeq[0][1] = 12;
    h->snapshotSlot[0][1] = 0;

    expect_count((int)harness_run(h, &dep),
                 (int)VRAM_FRESH_MATERIALIZED);
    expect_count(h->captures, 1);
    expect_count(h->writes, 512);
    expect_u64(h->matEpoch[0][0], 13, "captured tile epoch");
    expect_u64(h->matEpoch[0][1], 12, "previous snapshot epoch kept");

    harness_reset(h);
    memset(&dep, 0, sizeof(dep));
    append_tile_dep(&dep, 0, 0);
    append_tile_dep(&dep, 1, 0);
    append_tile_dep(&dep, 2, 0);

    h->physPresent[0][0] = 1;
    h->physSeq[0][0] = 13;
    h->snapshotPresent[0][1] = 1;
    h->snapshotFull[0][1] = 1;
    h->snapshotSeq[0][1] = 12;
    h->snapshotSlot[0][1] = 0;
    h->mapKind[0][1] = T6_MAP_PREVIOUS;
    h->tileMapId[0][1] = 2;
    h->snapshotPresent[0][2] = 1;
    h->snapshotFull[0][2] = 1;
    h->snapshotSeq[0][2] = 12;
    h->snapshotSlot[0][2] = 1;
    h->mapKind[0][2] = T6_MAP_PREVIOUS;
    h->tileMapId[0][2] = 3;
    expect_count((int)harness_run(h, &dep),
                 (int)VRAM_FRESH_UNRESOLVED);
    expect_count(h->captures, 0);
    expect_count(h->writes, 0);
    expect_u64(h->matEpoch[0][0], 0, "both slots required no epoch");
    expect_u64(h->matEpoch[0][1], 0, "previous epoch not committed");

    harness_reset(h);
    memset(&dep, 0, sizeof(dep));
    append_tile_dep(&dep, 0, 0);
    append_tile_dep(&dep, 1, 0);
    h->physPresent[0][0] = 1;
    h->physSeq[0][0] = 13;
    h->snapshotPresent[0][1] = 1;
    h->snapshotFull[0][1] = 1;
    h->snapshotSeq[0][1] = 12;
    h->snapshotSlot[0][1] = 0;
    h->mapKind[0][1] = T6_MAP_PREVIOUS;
    h->tileMapId[0][1] = 2;
    h->overwriteRequiredSlot = 1;
    expect_count((int)harness_run(h, &dep),
                 (int)VRAM_FRESH_UNRESOLVED);
    expect_count(h->captures, 1);
    expect_count(h->writes, 0);
    expect_u64(h->matEpoch[0][0], 0, "overwritten evidence no epoch");
    expect_u64(h->matEpoch[0][1], 0, "lost previous hazard no epoch");
}

static void test_materialize_core_baseline(void)
{
    T6Harness *h = &s_harness;
    VramReadDependency dep;

    harness_reset(h);
    memset(&dep, 0, sizeof(dep));
    append_tile_dep(&dep, 0, 0);

    h->physPresent[0][0] = 1;
    h->physSeq[0][0] = 13;
    h->snapshotPresent[0][0] = 1;
    h->snapshotFull[0][0] = 1;
    h->snapshotSeq[0][0] = 13;
    h->snapshotSlot[0][0] = 0;
    h->baselinePresent[0][0] = 1;
    h->baselineUsable[0][0] = 1;
    expect_count((int)harness_run(h, &dep),
                 (int)VRAM_FRESH_MATERIALIZED);
    expect_count(h->writes, 16);
    expect_u64(h->matEpoch[0][0], 13, "baseline delta commits epoch");

    harness_reset(h);
    h->physPresent[0][0] = 1;
    h->physSeq[0][0] = 13;
    h->snapshotPresent[0][0] = 1;
    h->snapshotFull[0][0] = 1;
    h->snapshotSeq[0][0] = 13;
    h->snapshotSlot[0][0] = 0;
    h->baselinePresent[0][0] = 1;
    h->baselineUsable[0][0] = 0;
    expect_count((int)harness_run(h, &dep),
                 (int)VRAM_FRESH_UNRESOLVED);
    expect_count(h->writes, 0);
    expect_u64(h->matEpoch[0][0], 0, "stale baseline no write");

    harness_reset(h);
    h->physPresent[0][0] = 1;
    h->physSeq[0][0] = 13;
    h->snapshotPresent[0][0] = 1;
    h->snapshotFull[0][0] = 1;
    h->snapshotSeq[0][0] = 13;
    h->snapshotSlot[0][0] = 0;
    expect_count((int)harness_run(h, &dep),
                 (int)VRAM_FRESH_MATERIALIZED);
    expect_count(h->writes, 256);
}

static void test_materialize_core_changed_runs(void)
{
    T6Harness *h = &s_harness;
    VramReadDependency dep;

    harness_reset(h);
    memset(&dep, 0, sizeof(dep));
    append_tile_dep(&dep, 0, 0);
    append_tile_dep(&dep, 1, 0);

    h->physPresent[0][0] = 1;
    h->physSeq[0][0] = 11;
    h->snapshotPresent[0][0] = 1;
    h->snapshotFull[0][0] = 1;
    h->snapshotSeq[0][0] = 11;
    h->snapshotSlot[0][0] = 0;
    h->physPresent[0][1] = 1;
    h->physSeq[0][1] = 11;
    h->snapshotPresent[0][1] = 1;
    h->snapshotFull[0][1] = 1;
    h->snapshotSeq[0][1] = 11;
    h->snapshotSlot[0][1] = 1;

    expect_count((int)harness_run(h, &dep),
                 (int)VRAM_FRESH_MATERIALIZED);
    expect_count(h->writes, 512);
    expect_count(h->invalidateRuns, 1);
}

static void test_map_classification(void)
{
    expect_count(T6ClassifyTileMap(0, 0, 0, 0, 320, 240,
                                   320, 0, 640, 240, 1),
                 T6_MAP_CURRENT);
    expect_count(T6ClassifyTileMap(19, 0, 0, 0, 320, 240,
                                   320, 0, 640, 240, 1),
                 T6_MAP_CURRENT);
    expect_count(T6ClassifyTileMap(20, 0, 0, 0, 320, 240,
                                   320, 0, 640, 240, 1),
                 T6_MAP_PREVIOUS);
    expect_count(T6ClassifyTileMap(20, 0, 0, 0, 336, 240,
                                   320, 0, 640, 240, 1),
                 T6_MAP_UNKNOWN);
    expect_count(T6ClassifyTileMap(0, 0, 0, 0, 320, 240,
                                   0, 0, 320, 240, 1),
                 T6_MAP_UNKNOWN);
    expect_count(T6ClassifyTileMap(20, 0, 0, 0, 320, 240,
                                   320, 0, 640, 240, 0),
                 T6_MAP_UNKNOWN);
}

static void test_physical_ownership(void)
{
    expect_bool(T6PhysicalOwnsTargetMap(1, T6_MAP_CURRENT, 1, 2), 1,
                "current owns target");
    expect_bool(T6PhysicalOwnsTargetMap(2, T6_MAP_CURRENT, 1, 2), 0,
                "current foreign rejected");
    expect_bool(T6PhysicalOwnsTargetMap(0, T6_MAP_PREVIOUS, 2, 2), 1,
                "pending owns previous");
    expect_bool(T6PhysicalOwnsTargetMap(0, T6_MAP_PREVIOUS, 3, 2), 0,
                "pending wrong previous rejected");
    expect_bool(T6PhysicalOwnsTargetMap(1, T6_MAP_PREVIOUS, 2, 2), 0,
                "active buffer not previous");
}

static void test_c0_hash_equivalence(void)
{
    uint16_t old[16][16];
    uint32_t before, after;
    int y, x;

    for (y = 0; y < 16; y++)
        for (x = 0; x < 16; x++)
            old[y][x] = 0x9000;

    before = 2166136261u;
    for (y = 0; y < 16; y++)
        for (x = 0; x < 16; x++)
            before = (before ^ old[y][x]) * 16777619u;

    for (y = 0; y < 16; y++)
        for (x = 0; x < 16; x++)
        {
            if (T6C0ColorCommitAllowed(11, 12))
                old[y][x] = T6MergePixelColor(old[y][x], 0x4321);
        }
    after = 2166136261u;
    for (y = 0; y < 16; y++)
        for (x = 0; x < 16; x++)
            after = (after ^ old[y][x]) * 16777619u;
    expect_bool(before == after, 1,
                "blocked C0 keeps hash");

    for (y = 0; y < 16; y++)
        for (x = 0; x < 16; x++)
            old[y][x] = 0x9000;
    for (y = 0; y < 16; y++)
        for (x = 0; x < 16; x++)
        {
            if (T6C0ColorCommitAllowed(13, 12))
                old[y][x] = T6MergePixelColor(old[y][x], 0x4321);
        }
    after = 2166136261u;
    for (y = 0; y < 16; y++)
        for (x = 0; x < 16; x++)
            after = (after ^ old[y][x]) * 16777619u;
    expect_bool(before != after, 1,
                "allowed C0 changes hash");
}

static void test_rebuild_baseline_gate(void)
{
    expect_bool(T6UploadAreaCoversMap(0, 0, 320, 240,
                                      0, 0, 320, 240), 1,
                "full-screen upload covers map");
    expect_bool(T6UploadAreaCoversMap(100, 100, 220, 180,
                                      0, 0, 320, 240), 0,
                "partial A0 never covers map");
    expect_bool(T6UploadAreaCoversMap(0, 0, 336, 240,
                                      0, 0, 320, 240), 1,
                "oversized upload still covers");

    expect_bool(T6RebuildCandidateEstablishes(1, 0, 1), 1,
                "candidate completion establishes");
    expect_bool(T6RebuildCandidateEstablishes(1, 1, 1), 0,
                "complete candidate not re-established");
    expect_bool(T6RebuildCandidateEstablishes(1, 0, 0), 0,
                "GX/clear FULL without A0 coverage rejected");
    expect_bool(T6RebuildCandidateEstablishes(0, 0, 1), 0,
                "invalid candidate rejected");
}

static void test_session_compact_gates(void)
{
    VramMaterializeSession s1;
    VramMaterializeSession s2;
    VramMaterializeWorkspace ws;
    VramReadDependency dep;
    int depth = 0;
    int ty, tx;

    memset(&ws, 0, sizeof(ws));
    expect_bool(sizeof(VramMaterializeSession) <= 16384, 1,
                "session size bounded");
    expect_bool(sizeof(VramMaterializeWorkspace) <= 65536, 1,
                "workspace size bounded");

    T6MaterializeSessionInit(&s1, &ws);
    T6MaterializeSessionObserveTile(&s1, 0, 0, 0, 11, 11, 1, 0,
                                    T6_MAP_CURRENT, 0, 0);
    expect_bool(T6MaterializeSessionHasEntry(&s1, 0, 0), 1,
                "entry recorded");
    T6MaterializeSessionInit(&s2, &ws);
    expect_bool(T6MaterializeSessionHasEntry(&s2, 0, 0), 0,
                "generation isolates sessions");

    expect_bool(T6WorkspaceBegin(&depth), 1, "workspace begin");
    expect_bool(T6WorkspaceBegin(&depth), 0, "reentrant rejected");
    T6WorkspaceEnd(&depth);
    expect_bool(T6WorkspaceBegin(&depth), 1, "workspace reusable");

    harness_reset(&s_harness);
    memset(&dep, 0, sizeof(dep));
    for (tx = 0; tx < 12; tx++)
    {
        VramRect r = { 0, 0, 1024, 512 };

        VramReadDependencyAppend(&dep, &r);
    }
    for (ty = 0; ty < T6_VRAM_TILE_Y; ty++)
        for (tx = 0; tx < T6_VRAM_TILE_X; tx++)
        {
            s_harness.snapshotPresent[ty][tx] = 1;
            s_harness.snapshotFull[ty][tx] = 1;
            s_harness.snapshotSeq[ty][tx] = 11;
            s_harness.snapshotSlot[ty][tx] = (tx + ty) & 1;
        }
    expect_count((int)harness_run(&s_harness, &dep),
                 (int)VRAM_FRESH_MATERIALIZED);
    expect_count(s_harness.touchedCount, 2048);
    expect_bool(s_harness.overflow == 0, 1,
                "max dependency no overflow");
}

static void test_window_lookup_source_only(void)
{
    T6Harness *h = &s_harness;
    VramFreshResult r;
    int hit = 0;

    harness_reset(h);
    harness_set_tile(h, 0, 0, 11);

    r = simulate_window_lookup(h, 0, 2, 0, &hit);
    expect_count((int)r, (int)VRAM_FRESH_MATERIALIZED);
    expect_count(hit, 0);
    expect_count(h->wndUploads, 1);
    expect_count(h->wndHits, 0);
    expect_count(h->captures, 0);
    expect_bool(h->writes > 0, 1, "source pixels written");
    expect_bool(h->invalidateRuns >= 1, 1, "source invalidation run");
    expect_u64(h->matEpoch[0][0], 11, "source epoch");

    hit = 0;
    r = simulate_window_lookup(h, 0, 2, 0, &hit);
    expect_count((int)r, (int)VRAM_FRESH_NO_ACTION);
    expect_count(hit, 1);
    expect_count(h->wndUploads, 1);
    expect_count(h->wndHits, 1);
}

static void test_window_lookup_clut4_order(void)
{
    T6Harness *h = &s_harness;
    VramFreshResult r;
    uint32_t initialChecksum;
    uint32_t keyChecksum;
    int hit = 0;

    harness_reset(h);
    initialChecksum = production_palette_checksum(h, 0x3F, 0);
    harness_set_tile(h, 63, 0, 11);

    r = simulate_window_lookup(h, 0, 0, 0x3F, &hit);
    expect_count((int)r, (int)VRAM_FRESH_MATERIALIZED);
    expect_count(hit, 0);
    expect_count(h->wndUploads, 1);
    expect_u64(h->matEpoch[0][63], 11, "clut4 epoch");
    keyChecksum = (h->wndKey[0] >> 16) & 0x3FFF;
    expect_bool(keyChecksum == production_palette_checksum(h, 0x3F, 0), 1,
                "clut4 key uses post-barrier checksum");
    expect_bool(keyChecksum != initialChecksum, 1,
                "clut4 barrier injected B checksum");

    hit = 0;
    r = simulate_window_lookup(h, 0, 0, 0x3F, &hit);
    expect_count((int)r, (int)VRAM_FRESH_NO_ACTION);
    expect_count(hit, 1);
    expect_count(h->wndUploads, 1);
    expect_count(h->wndHits, 1);
}

static void test_window_lookup_clut8_order(void)
{
    T6Harness *h = &s_harness;
    VramFreshResult r;
    uint32_t initialChecksum;
    uint32_t keyChecksum;
    int hit = 0;

    harness_reset(h);
    initialChecksum = production_palette_checksum(h, 0x3F, 1);
    harness_set_tile(h, 63, 0, 11);

    r = simulate_window_lookup(h, 0, 1, 0x3F, &hit);
    expect_count((int)r, (int)VRAM_FRESH_MATERIALIZED);
    expect_count(hit, 0);
    expect_count(h->wndUploads, 1);
    expect_u64(h->matEpoch[0][63], 11, "clut8 epoch");
    keyChecksum = (h->wndKey[0] >> 16) & 0x3FFF;
    expect_bool(keyChecksum == production_palette_checksum(h, 0x3F, 1), 1,
                "clut8 key uses post-barrier checksum");
    expect_bool(keyChecksum != initialChecksum, 1,
                "clut8 barrier injected B checksum");

    hit = 0;
    r = simulate_window_lookup(h, 0, 1, 0x3F, &hit);
    expect_count((int)r, (int)VRAM_FRESH_NO_ACTION);
    expect_count(hit, 1);
    expect_count(h->wndUploads, 1);
    expect_count(h->wndHits, 1);
}

static void test_window_lookup_preset_hit(void)
{
    T6Harness *h = &s_harness;
    VramFreshResult r;
    unsigned int key;
    int hit = 0;

    harness_reset(h);
    key = (0x3F & 0x7FFF) |
          (((uint32_t)production_palette_checksum(h, 0x3F, 0)) << 16);
    h->wndUsed[0] = 1;
    h->wndKey[0] = key;
    h->wndPage[0] = 0;
    h->wndMode[0] = 0;
    h->wndClut[0] = 0x3F;
    h->wndCount = 1;

    r = simulate_window_lookup(h, 0, 0, 0x3F, &hit);
    expect_count((int)r, (int)VRAM_FRESH_NO_ACTION);
    expect_count(hit, 1);
    expect_count(h->wndUploads, 0);
    expect_count(h->captures, 0);

    hit = 0;
    r = simulate_window_lookup(h, 0, 0, 0x3F, &hit);
    expect_count((int)r, (int)VRAM_FRESH_NO_ACTION);
    expect_count(hit, 1);
    expect_count(h->wndHits, 2);
    expect_count(h->wndUploads, 0);
}

static void test_window_lookup_preset_miss(void)
{
    T6Harness *h = &s_harness;
    VramFreshResult r;
    int hit = 0;

    harness_reset(h);
    harness_set_tile(h, 63, 0, 11);

    r = simulate_window_lookup(h, 0, 0, 0x3F, &hit);
    expect_count((int)r, (int)VRAM_FRESH_MATERIALIZED);
    expect_count(hit, 0);
    expect_count(h->wndUploads, 1);
    expect_count(h->wndHits, 0);

    hit = 0;
    r = simulate_window_lookup(h, 0, 0, 0x3F, &hit);
    expect_count((int)r, (int)VRAM_FRESH_NO_ACTION);
    expect_count(hit, 1);
    expect_count(h->wndUploads, 1);
    expect_count(h->wndHits, 1);
}

static void test_window_lookup_stale_hit_source_capture(void)
{
    T6Harness *h = &s_harness;
    VramFreshResult r;
    int hit = 0;

    harness_reset(h);
    h->wndUsed[0] = 1;
    h->wndKey[0] = 0;
    h->wndPage[0] = 0;
    h->wndMode[0] = 2;
    h->wndClut[0] = 0;
    h->wndCount = 1;
    harness_set_phys_only(h, 0, 0, 11);

    r = simulate_window_lookup(h, 0, 2, 0, &hit);
    expect_count((int)r, (int)VRAM_FRESH_MATERIALIZED);
    expect_count(h->captures, 1);
    expect_count(hit, 0);
    expect_count(h->wndUploads, 1);
    expect_bool(h->wndUsed[0] == 0, 1,
                "stale source entry invalidated");
    expect_bool(h->writes > 0, 1, "source captured pixels");

    hit = 0;
    r = simulate_window_lookup(h, 0, 2, 0, &hit);
    expect_count((int)r, (int)VRAM_FRESH_NO_ACTION);
    expect_count(h->captures, 1);
    expect_count(hit, 1);
    expect_count(h->wndUploads, 1);
}

static void test_window_lookup_stale_hit_clut_capture(void)
{
    T6Harness *h = &s_harness;
    VramFreshResult r;
    unsigned int initialKey;
    int hit = 0;

    harness_reset(h);
    initialKey = (0x3F & 0x7FFF) |
                 (((uint32_t)production_palette_checksum(
                     h, 0x3F, 0)) << 16);
    h->wndUsed[0] = 1;
    h->wndKey[0] = initialKey;
    h->wndPage[0] = 0;
    h->wndMode[0] = 0;
    h->wndClut[0] = 0x3F;
    h->wndCount = 1;
    harness_set_phys_only(h, 63, 0, 11);

    r = simulate_window_lookup(h, 0, 0, 0x3F, &hit);
    expect_count((int)r, (int)VRAM_FRESH_MATERIALIZED);
    expect_count(h->captures, 1);
    expect_count(hit, 0);
    expect_count(h->wndUploads, 1);
    expect_bool(h->wndUsed[0] == 0, 1,
                "stale clut entry invalidated");
    expect_u64(h->matEpoch[0][63], 11, "clut capture epoch");

    hit = 0;
    r = simulate_window_lookup(h, 0, 0, 0x3F, &hit);
    expect_count((int)r, (int)VRAM_FRESH_NO_ACTION);
    expect_count(h->captures, 1);
    expect_count(hit, 1);
    expect_count(h->wndUploads, 1);
}

static void test_production_checksum_vector(void)
{
    T6Harness *h = &s_harness;
    int i;

    harness_reset(h);
    for (i = 0; i < 16; i++)
        h->vram[i] = 0x0000;
    h->vram[0] = 0x0000;
    h->vram[1] = 0x0001;
    h->vram[2] = 0x0002;
    h->vram[3] = 0x0000;

    /* Hand-computed mode 0 vector with unsigned 32-bit wrap:
     * v1=0x00010000, v2=0x00000002, v3..v8=0
     * rows 3..8 contribute (0xFFFFFFFF<<row) mod 2^32,
     * sum=0x0001FE0A, fold -> 0x0001FE0B, &0x3fff -> 0x3E0B */
    expect_u64(production_palette_checksum(h, 0, 0), 0x3E0Bu,
               "fixed checksum vector");
}

static void test_capture_failure_reason(void)
{
    expect_count(T6ClassifyCaptureFailureReason(1, 0, 0, 0, 0),
                 T6_REASON_RGB24);
    expect_count(T6ClassifyCaptureFailureReason(0, 1, 0, 0, 0),
                 T6_REASON_HAZARD);
    expect_count(T6ClassifyCaptureFailureReason(0, 0, 1, 0, 0),
                 T6_REASON_HAZARD);
    expect_count(T6ClassifyCaptureFailureReason(0, 0, 0, 1, 0),
                 T6_REASON_HAZARD);
    expect_count(T6ClassifyCaptureFailureReason(0, 0, 0, 0, 1),
                 T6_REASON_UNKNOWN_MAP);
    expect_count(T6ClassifyCaptureFailureReason(0, 0, 0, 0, 0),
                 T6_REASON_CAPTURE_FAIL);
}

static void test_window_lookup_harness_cumulative_capture(void)
{
    T6Harness *h = &s_harness;
    VramFreshResult r;
    int hit = 0;

    harness_reset(h);
    harness_set_phys_only(h, 0, 0, 11);

    r = simulate_window_lookup(h, 0, 2, 0, &hit);
    expect_count((int)r, (int)VRAM_FRESH_MATERIALIZED);
    expect_count(h->captures, 1);
    expect_count(h->wndUploads, 1);

    /* Harness-level behavior only; the production CopyTex total wiring is
     * covered separately by test_copytex_stats_shared. */
    harness_set_phys_only(h, 1, 0, 12);
    hit = 0;
    r = simulate_window_lookup(h, 0, 2, 0, &hit);
    expect_count((int)r, (int)VRAM_FRESH_MATERIALIZED);
    expect_count(h->captures, 2);
    expect_count(h->wndUploads, 2);

    /* Both tiles fresh now: NO_ACTION, captures stay cumulative. */
    hit = 0;
    r = simulate_window_lookup(h, 0, 2, 0, &hit);
    expect_count((int)r, (int)VRAM_FRESH_NO_ACTION);
    expect_count(h->captures, 2);
    expect_count(hit, 1);
    expect_count(h->wndUploads, 2);
}

static void test_capture_diag_production_flow(void)
{
    T6CaptureDiagInput diag;

    /* Current RGB15 capture, unrelated pending map RGB24: must stay
     * CAPTURE_FAIL, not leak the pending map's RGB24 flag. */
    T6CaptureDiagFill(&diag, 0, 0, 1, T6_MAP_CURRENT, 0, 0, 0, 0);
    expect_count(T6ClassifyCaptureFailure(&diag),
                 T6_REASON_CAPTURE_FAIL);

    /* Previous-map capture really reads the pending RGB24 state. */
    T6CaptureDiagFill(&diag, 0, 0, 1, T6_MAP_PREVIOUS, 0, 0, 0, 0);
    expect_count(T6ClassifyCaptureFailure(&diag), T6_REASON_RGB24);

    /* Global RGB24 affects both mappings. */
    T6CaptureDiagFill(&diag, 1, 0, 0, T6_MAP_CURRENT, 0, 0, 0, 0);
    expect_count(T6ClassifyCaptureFailure(&diag), T6_REASON_RGB24);

    /* Hazard flags survive the diag input path. */
    T6CaptureDiagFill(&diag, 0, 0, 0, T6_MAP_CURRENT, 1, 0, 0, 0);
    expect_count(T6ClassifyCaptureFailure(&diag), T6_REASON_HAZARD);
    T6CaptureDiagFill(&diag, 0, 0, 0, T6_MAP_CURRENT, 0, 1, 0, 0);
    expect_count(T6ClassifyCaptureFailure(&diag), T6_REASON_HAZARD);
    T6CaptureDiagFill(&diag, 0, 0, 0, T6_MAP_CURRENT, 0, 0, 1, 0);
    expect_count(T6ClassifyCaptureFailure(&diag), T6_REASON_HAZARD);
}

static void test_copytex_stats_shared(void)
{
    unsigned int thisCall = 0;
    unsigned int total = 0;

    T6CopyTexBeginCall(&thisCall);
    T6CopyTexCapture(&thisCall, &total);
    T6CopyTexCapture(&thisCall, &total);
    expect_count((int)thisCall, 2);
    expect_count((int)total, 2);

    /* Next barrier starts with a fresh delta but keeps the cumulative total. */
    T6CopyTexBeginCall(&thisCall);
    expect_count((int)thisCall, 0);
    expect_count((int)total, 2);
}

static void test_standard_lookup_cache_miss(void)
{
    T6Harness *h = &s_harness;
    VramFreshResult r;
    int hit = 0;

    harness_reset(h);
    harness_set_tile(h, 0, 0, 11);

    r = simulate_standard_lookup(h, 0, 2, 0, 0, 255, 0, 255, 0, &hit);
    expect_count((int)r, (int)VRAM_FRESH_MATERIALIZED);
    expect_count(hit, 0);
    expect_count(h->stdUploads, 1);
    expect_count(h->stdHits, 0);
    expect_u64(h->matEpoch[0][0], 11, "standard source epoch");

    hit = 0;
    r = simulate_standard_lookup(h, 0, 2, 0, 0, 255, 0, 255, 0, &hit);
    expect_count((int)r, (int)VRAM_FRESH_NO_ACTION);
    expect_count(hit, 1);
    expect_count(h->stdUploads, 1);
    expect_count(h->stdHits, 1);
}

static void test_standard_lookup_preset_hit(void)
{
    T6Harness *h = &s_harness;
    VramFreshResult r;
    unsigned int key;
    int hit = 0;

    harness_reset(h);
    key = (0x3F & 0x7FFF) |
          (((uint32_t)production_palette_checksum(h, 0x3F, 0)) << 16);
    h->stdUsed[0] = 1;
    h->stdKey[0] = key;
    h->stdPage[0] = 0;
    h->stdMode[0] = 0;
    h->stdClut[0] = 0x3F;
    h->stdU0[0] = 0;
    h->stdU1[0] = 15;
    h->stdV0[0] = 0;
    h->stdV1[0] = 1;
    h->stdIL[0] = 0;
    h->stdCount = 1;

    r = simulate_standard_lookup(h, 0, 0, 0x3F, 0, 15, 0, 1, 0, &hit);
    expect_count((int)r, (int)VRAM_FRESH_NO_ACTION);
    expect_count(hit, 1);
    expect_count(h->stdUploads, 0);
    expect_count(h->captures, 0);
}

static void test_standard_lookup_clut4_palette(void)
{
    T6Harness *h = &s_harness;
    VramFreshResult r;
    int hit = 0;

    harness_reset(h);
    harness_set_tile(h, 63, 0, 11);

    r = simulate_standard_lookup(h, 0, 0, 0x3F, 0, 15, 0, 1, 0, &hit);
    expect_count((int)r, (int)VRAM_FRESH_MATERIALIZED);
    expect_count(hit, 0);
    expect_count(h->stdUploads, 1);
    expect_u64(h->matEpoch[0][63], 11, "standard clut4 epoch");

    hit = 0;
    r = simulate_standard_lookup(h, 0, 0, 0x3F, 0, 15, 0, 1, 0, &hit);
    expect_count((int)r, (int)VRAM_FRESH_NO_ACTION);
    expect_count(hit, 1);
    expect_count(h->stdUploads, 1);
}

static void test_standard_lookup_clut8_palette(void)
{
    T6Harness *h = &s_harness;
    VramFreshResult r;
    int hit = 0;

    harness_reset(h);
    harness_set_tile(h, 63, 0, 11);

    r = simulate_standard_lookup(h, 0, 1, 0x3F, 0, 31, 0, 1, 0, &hit);
    expect_count((int)r, (int)VRAM_FRESH_MATERIALIZED);
    expect_count(hit, 0);
    expect_count(h->stdUploads, 1);
    expect_u64(h->matEpoch[0][63], 11, "standard clut8 epoch");

    hit = 0;
    r = simulate_standard_lookup(h, 0, 1, 0x3F, 0, 31, 0, 1, 0, &hit);
    expect_count((int)r, (int)VRAM_FRESH_NO_ACTION);
    expect_count(hit, 1);
    expect_count(h->stdUploads, 1);
}

static void test_standard_lookup_capture_suppression(void)
{
    T6Harness *h = &s_harness;
    VramFreshResult r;
    int hit = 0;

    harness_reset(h);
    harness_set_phys_only(h, 0, 0, 11);

    r = simulate_standard_lookup(h, 0, 2, 0, 0, 255, 0, 255, 0, &hit);
    expect_count((int)r, (int)VRAM_FRESH_MATERIALIZED);
    expect_count(h->captures, 1);
    expect_count(hit, 0);
    expect_count(h->stdUploads, 1);

    hit = 0;
    r = simulate_standard_lookup(h, 0, 2, 0, 0, 255, 0, 255, 0, &hit);
    expect_count((int)r, (int)VRAM_FRESH_NO_ACTION);
    expect_count(h->captures, 1);
    expect_count(hit, 1);
    expect_count(h->stdUploads, 1);
}

static void test_standard_lookup_identity(void)
{
    T6Harness *h = &s_harness;
    VramFreshResult r;
    int hit = 0;

    harness_reset(h);
    h->stdUsed[0] = 1;
    h->stdKey[0] = 0;
    h->stdPage[0] = 0;
    h->stdMode[0] = 2;
    h->stdClut[0] = 0;
    h->stdU0[0] = 0;
    h->stdU1[0] = 255;
    h->stdV0[0] = 0;
    h->stdV1[0] = 255;
    h->stdIL[0] = 0;
    h->stdCount = 1;

    /* Same CLUT key but different page must miss. */
    r = simulate_standard_lookup(h, 1, 2, 0, 0, 255, 0, 255, 0, &hit);
    expect_count((int)r, (int)VRAM_FRESH_NO_ACTION);
    expect_count(hit, 0);
    expect_count(h->stdUploads, 1);

    harness_reset(h);
    h->stdUsed[0] = 1;
    h->stdKey[0] = 0;
    h->stdPage[0] = 0;
    h->stdMode[0] = 2;
    h->stdU0[0] = 0;
    h->stdU1[0] = 255;
    h->stdV0[0] = 0;
    h->stdV1[0] = 255;
    h->stdIL[0] = 0;
    h->stdCount = 1;
    hit = 0;
    r = simulate_standard_lookup(h, 0, 0, 0x3F, 0, 15, 0, 1, 0, &hit);
    expect_count((int)r, (int)VRAM_FRESH_NO_ACTION);
    expect_count(hit, 0);
    expect_count(h->stdUploads, 1);

    harness_reset(h);
    h->stdUsed[0] = 1;
    h->stdKey[0] = (0x3F & 0x7FFF) |
                   (((uint32_t)production_palette_checksum(
                       h, 0x3F, 0)) << 16);
    h->stdPage[0] = 0;
    h->stdMode[0] = 0;
    h->stdClut[0] = 0x3F;
    h->stdU0[0] = 0;
    h->stdU1[0] = 15;
    h->stdV0[0] = 0;
    h->stdV1[0] = 1;
    h->stdIL[0] = 0;
    h->stdCount = 1;
    hit = 0;
    r = simulate_standard_lookup(h, 0, 0, 0x3F, 16, 31, 0, 1, 0, &hit);
    expect_count((int)r, (int)VRAM_FRESH_NO_ACTION);
    expect_count(hit, 0);
    expect_count(h->stdUploads, 1);

    harness_reset(h);
    h->stdUsed[0] = 1;
    h->stdKey[0] = 0;
    h->stdPage[0] = 0;
    h->stdMode[0] = 2;
    h->stdU0[0] = 0;
    h->stdU1[0] = 255;
    h->stdV0[0] = 0;
    h->stdV1[0] = 255;
    h->stdIL[0] = 0;
    h->stdCount = 1;
    hit = 0;
    r = simulate_standard_lookup(h, 0, 2, 0, 0, 255, 0, 255, 1, &hit);
    expect_count((int)r, (int)VRAM_FRESH_NO_ACTION);
    expect_count(hit, 1);
    expect_count(h->stdUploads, 0);
}

static void test_standard_lookup_stale_hit_source(void)
{
    T6Harness *h = &s_harness;
    VramFreshResult r;
    uint32_t targetBefore, bgBefore;
    int hit = 0;

    harness_reset(h);
    h->stdUsed[0] = 1;
    h->stdKey[0] = 0;
    h->stdPage[0] = 0;
    h->stdMode[0] = 2;
    h->stdClut[0] = 0;
    h->stdU0[0] = 0;
    h->stdU1[0] = 255;
    h->stdV0[0] = 0;
    h->stdV1[0] = 255;
    h->stdIL[0] = 0;
    h->stdCount = 1;
    harness_set_phys_only(h, 0, 0, 11);
    targetBefore = tile_hash(h, 0, 0);
    bgBefore = tile_hash(h, 5, 5);

    r = simulate_standard_lookup(h, 0, 2, 0, 0, 255, 0, 255, 0, &hit);
    expect_count((int)r, (int)VRAM_FRESH_MATERIALIZED);
    expect_count(h->captures, 1);
    expect_count(hit, 0);
    expect_count(h->stdUploads, 1);
    expect_bool(h->stdUsed[0] == 0, 1,
                "stale source standard entry invalidated");
    expect_bool(tile_hash(h, 0, 0) != targetBefore, 1,
                "source tile carries GX pixel");
    expect_bool(tile_hash(h, 5, 5) == bgBefore, 1,
                "source background hash unchanged");

    hit = 0;
    r = simulate_standard_lookup(h, 0, 2, 0, 0, 255, 0, 255, 0, &hit);
    expect_count((int)r, (int)VRAM_FRESH_NO_ACTION);
    expect_count(h->captures, 1);
    expect_count(hit, 1);
    expect_count(h->stdUploads, 1);
}

static void test_standard_lookup_stale_hit_clut4(void)
{
    T6Harness *h = &s_harness;
    VramFreshResult r;
    uint32_t palBefore, bgBefore, checksumBefore;
    unsigned int initialKey;
    int hit = 0;

    harness_reset(h);
    initialKey = (0x3F & 0x7FFF) |
                 (((uint32_t)production_palette_checksum(
                     h, 0x3F, 0)) << 16);
    h->stdUsed[0] = 1;
    h->stdKey[0] = initialKey;
    h->stdPage[0] = 0;
    h->stdMode[0] = 0;
    h->stdClut[0] = 0x3F;
    h->stdU0[0] = 0;
    h->stdU1[0] = 15;
    h->stdV0[0] = 0;
    h->stdV1[0] = 1;
    h->stdIL[0] = 0;
    h->stdCount = 1;
    harness_set_phys_only(h, 63, 0, 11);
    palBefore = tile_hash(h, 63, 0);
    bgBefore = tile_hash(h, 5, 5);
    checksumBefore = production_palette_checksum(h, 0x3F, 0);

    r = simulate_standard_lookup(h, 0, 0, 0x3F, 0, 15, 0, 1, 0, &hit);
    expect_count((int)r, (int)VRAM_FRESH_MATERIALIZED);
    expect_count(h->captures, 1);
    expect_count(hit, 0);
    expect_count(h->stdUploads, 1);
    expect_bool(h->stdUsed[0] == 0, 1,
                "stale clut4 standard entry invalidated");
    expect_bool(tile_hash(h, 63, 0) != palBefore, 1,
                "clut4 palette carries GX word");
    expect_bool(tile_hash(h, 5, 5) == bgBefore, 1,
                "clut4 background hash unchanged");
    expect_bool(production_palette_checksum(h, 0x3F, 0) != checksumBefore,
                1, "clut4 checksum changed");

    hit = 0;
    r = simulate_standard_lookup(h, 0, 0, 0x3F, 0, 15, 0, 1, 0, &hit);
    expect_count((int)r, (int)VRAM_FRESH_NO_ACTION);
    expect_count(h->captures, 1);
    expect_count(hit, 1);
    expect_count(h->stdUploads, 1);
}

static void test_standard_lookup_stale_hit_clut8(void)
{
    T6Harness *h = &s_harness;
    VramFreshResult r;
    uint32_t palBefore, bgBefore, checksumBefore;
    unsigned int initialKey;
    int hit = 0;

    harness_reset(h);
    initialKey = (0x3F & 0x7FFF) |
                 (((uint32_t)production_palette_checksum(
                     h, 0x3F, 1)) << 16);
    h->stdUsed[0] = 1;
    h->stdKey[0] = initialKey;
    h->stdPage[0] = 0;
    h->stdMode[0] = 1;
    h->stdClut[0] = 0x3F;
    h->stdU0[0] = 0;
    h->stdU1[0] = 31;
    h->stdV0[0] = 0;
    h->stdV1[0] = 1;
    h->stdIL[0] = 0;
    h->stdCount = 1;
    harness_set_phys_only(h, 63, 0, 11);
    palBefore = tile_hash(h, 63, 0);
    bgBefore = tile_hash(h, 5, 5);
    checksumBefore = production_palette_checksum(h, 0x3F, 1);

    r = simulate_standard_lookup(h, 0, 1, 0x3F, 0, 31, 0, 1, 0, &hit);
    expect_count((int)r, (int)VRAM_FRESH_MATERIALIZED);
    expect_count(h->captures, 1);
    expect_count(hit, 0);
    expect_count(h->stdUploads, 1);
    expect_bool(h->stdUsed[0] == 0, 1,
                "stale clut8 standard entry invalidated");
    expect_bool(tile_hash(h, 63, 0) != palBefore, 1,
                "clut8 palette carries GX word");
    expect_bool(tile_hash(h, 5, 5) == bgBefore, 1,
                "clut8 background hash unchanged");
    expect_bool(production_palette_checksum(h, 0x3F, 1) != checksumBefore,
                1, "clut8 checksum changed");

    hit = 0;
    r = simulate_standard_lookup(h, 0, 1, 0x3F, 0, 31, 0, 1, 0, &hit);
    expect_count((int)r, (int)VRAM_FRESH_NO_ACTION);
    expect_count(h->captures, 1);
    expect_count(hit, 1);
    expect_count(h->stdUploads, 1);
}

static void test_standard_lookup_stale_hit_interleaved_mode0(void)
{
    T6Harness *h = &s_harness;
    VramFreshResult r;
    uint32_t targetBefore, bgBefore;
    unsigned int initialKey;
    int hit = 0;

    harness_reset(h);
    initialKey = (0 & 0x7FFF) |
                 (((uint32_t)production_palette_checksum(
                     h, 0, 0)) << 16);
    h->stdUsed[0] = 1;
    h->stdKey[0] = initialKey;
    h->stdPage[0] = 0;
    h->stdMode[0] = 0;
    h->stdClut[0] = 0;
    h->stdU0[0] = 0;
    h->stdU1[0] = 3;
    h->stdV0[0] = 15;
    h->stdV1[0] = 15;
    h->stdIL[0] = 0;
    h->stdCount = 1;
    h->stdUsed[1] = 1;
    h->stdKey[1] = 0;
    h->stdPage[1] = 1;
    h->stdMode[1] = 0;
    h->stdClut[1] = 0;
    h->stdU0[1] = 0;
    h->stdU1[1] = 15;
    h->stdV0[1] = 0;
    h->stdV1[1] = 1;
    h->stdIL[1] = 0;
    h->stdCount = 2;

    /* u=0,v=15 mode 0 swizzles to word x=60,y=0 (tile 3,0), while the linear
     * UV rect lives in tile 0,0; only full page/mode invalidation can clear
     * this preset entry. */
    harness_set_phys_only(h, 3, 0, 11);
    targetBefore = tile_hash(h, 3, 0);
    bgBefore = tile_hash(h, 5, 5);

    r = simulate_standard_lookup(h, 0, 0, 0, 0, 3, 15, 15, 1, &hit);
    expect_count((int)r, (int)VRAM_FRESH_MATERIALIZED);
    expect_count(h->captures, 1);
    expect_count(hit, 0);
    expect_count(h->stdUploads, 1);
    expect_bool(h->stdUsed[0] == 0, 1,
                "interleaved mode0 stale entry invalidated");
    expect_bool(h->stdUsed[1] == 1, 1,
                "unrelated page/mode entry kept");
    expect_bool(tile_hash(h, 3, 0) != targetBefore, 1,
                "interleaved mode0 swizzle tile carries GX pixel");
    expect_bool(tile_hash(h, 5, 5) == bgBefore, 1,
                "interleaved mode0 background unchanged");

    hit = 0;
    r = simulate_standard_lookup(h, 0, 0, 0, 0, 3, 15, 15, 1, &hit);
    expect_count((int)r, (int)VRAM_FRESH_NO_ACTION);
    expect_count(h->captures, 1);
    expect_count(hit, 1);
    expect_count(h->stdUploads, 1);
}

static void test_standard_lookup_stale_hit_interleaved_mode1(void)
{
    T6Harness *h = &s_harness;
    VramFreshResult r;
    uint32_t targetBefore, bgBefore;
    unsigned int initialKey;
    int hit = 0;

    harness_reset(h);
    initialKey = (0 & 0x7FFF) |
                 (((uint32_t)production_palette_checksum(
                     h, 0, 1)) << 16);
    h->stdUsed[0] = 1;
    h->stdKey[0] = initialKey;
    h->stdPage[0] = 17;
    h->stdMode[0] = 1;
    h->stdClut[0] = 0;
    h->stdU0[0] = 0;
    h->stdU1[0] = 31;
    h->stdV0[0] = 7;
    h->stdV1[0] = 7;
    h->stdIL[0] = 0;
    h->stdCount = 1;

    /* Page 17 mode 1: the interleaved loader reads the whole page while the
     * linear UV rect only covers tile 4,16.  Tile 5,16 is inside the
     * swizzled page footprint but outside both the linear UV rect and the
     * CLUT row, so only the conservative mode/page invalidation can clear
     * this preset entry. */
    harness_set_phys_only(h, 5, 16, 11);
    targetBefore = tile_hash(h, 5, 16);
    bgBefore = tile_hash(h, 5, 5);

    r = simulate_standard_lookup(h, 17, 1, 0, 0, 31, 7, 7, 1, &hit);
    expect_count((int)r, (int)VRAM_FRESH_MATERIALIZED);
    expect_count(h->captures, 1);
    expect_count(hit, 0);
    expect_count(h->stdUploads, 1);
    expect_bool(h->stdUsed[0] == 0, 1,
                "interleaved mode1 stale entry invalidated");
    expect_bool(tile_hash(h, 5, 16) != targetBefore, 1,
                "interleaved mode1 swizzle tile carries GX pixel");
    expect_bool(tile_hash(h, 5, 5) == bgBefore, 1,
                "interleaved mode1 background unchanged");

    hit = 0;
    r = simulate_standard_lookup(h, 17, 1, 0, 0, 31, 7, 7, 1, &hit);
    expect_count((int)r, (int)VRAM_FRESH_NO_ACTION);
    expect_count(h->captures, 1);
    expect_count(hit, 1);
    expect_count(h->stdUploads, 1);
}

typedef struct T6PageCoreCallRecord
{
    int pageid[4];
    int mode[4];
    int partition[4];
    unsigned int packed[4];
    int count;
} T6PageCoreCallRecord;

static void record_standard_page_call(void *user, int pageid, int mode,
                                      int partition,
                                      unsigned int packedPos)
{
    T6PageCoreCallRecord *rec = (T6PageCoreCallRecord *)user;

    if (rec == NULL || rec->count >= 4)
        return;
    rec->pageid[rec->count] = pageid;
    rec->mode[rec->count] = mode;
    rec->partition[rec->count] = partition;
    rec->packed[rec->count] = packedPos;
    rec->count++;
}

static void test_standard_page_invalidate_core(void)
{
    T6PageCoreCallRecord calls;
    unsigned int entryPos;
    unsigned int clampedNpos;
    int i;

    memset(&calls, 0, sizeof(calls));
    T6StandardPageInvalidateCore(
        15, 1, record_standard_page_call, &calls);
    expect_count(calls.count, 4);
    for (i = 0; i < 4; i++)
    {
        expect_count(calls.pageid[i], 15);
        expect_count(calls.mode[i], 1);
        expect_count(calls.partition[i], i);
        expect_u64((uint64_t)calls.packed[i], 0x00FF00FFu,
                   "full page packed range");
    }

    /* A u=128..255 entry must intersect the full page but not a clamped
     * [0,127] npos, which is exactly the right-edge regression. */
    entryPos = T6StandardEntryPackedPos(128, 255, 0, 255);
    expect_bool(T6PackedPosIntersects(entryPos, 0x00FF00FFu), 1,
                "wrapped UV half intersects full page");
    clampedNpos = 0x007F00FFu; /* x2=127 */
    expect_bool(T6PackedPosIntersects(entryPos, clampedNpos), 0,
                "clamped right edge misses wrapped UV half");

    /* Invalid page/mode must not invoke the callback. */
    memset(&calls, 0, sizeof(calls));
    T6StandardPageInvalidateCore(32, 0, record_standard_page_call, &calls);
    expect_count(calls.count, 0);
    T6StandardPageInvalidateCore(0, 3, record_standard_page_call, &calls);
    expect_count(calls.count, 0);
}

static void test_standard_lookup_same_color_interleaved_mode0(void)
{
    T6Harness *h = &s_harness;
    VramFreshResult r;
    unsigned int initialKey;
    int hit = 0;

    harness_reset(h);
    initialKey = (0 & 0x7FFF) |
                 (((uint32_t)production_palette_checksum(
                     h, 0, 0)) << 16);
    h->stdUsed[0] = 1;
    h->stdKey[0] = initialKey;
    h->stdPage[0] = 0;
    h->stdMode[0] = 0;
    h->stdClut[0] = 0;
    h->stdU0[0] = 0;
    h->stdU1[0] = 3;
    h->stdV0[0] = 15;
    h->stdV1[0] = 15;
    h->stdIL[0] = 0;
    h->stdCount = 1;

    /* Pre-fill the swizzle target tile with exactly what the harness would
     * materialize, so changedCount stays zero. */
    harness_prefill_tile(h, 3, 0);
    harness_set_phys_only(h, 3, 0, 11);

    r = simulate_standard_lookup(h, 0, 0, 0, 0, 3, 15, 15, 1, &hit);
    expect_count((int)r, (int)VRAM_FRESH_MATERIALIZED);
    expect_count(h->captures, 1);
    expect_count(hit, 1);
    expect_count(h->stdUploads, 0);
    expect_bool(h->stdUsed[0] == 1, 1,
                "same-color mode0 keeps stale entry");
    expect_u64(h->matEpoch[0][3], 11, "same-color mode0 commits epoch");

    hit = 0;
    r = simulate_standard_lookup(h, 0, 0, 0, 0, 3, 15, 15, 1, &hit);
    expect_count((int)r, (int)VRAM_FRESH_NO_ACTION);
    expect_count(h->captures, 1);
    expect_count(hit, 1);
    expect_count(h->stdUploads, 0);
}

static void test_standard_lookup_same_color_interleaved_mode1(void)
{
    T6Harness *h = &s_harness;
    VramFreshResult r;
    unsigned int initialKey;
    int hit = 0;

    harness_reset(h);
    initialKey = (0 & 0x7FFF) |
                 (((uint32_t)production_palette_checksum(
                     h, 0, 1)) << 16);
    h->stdUsed[0] = 1;
    h->stdKey[0] = initialKey;
    h->stdPage[0] = 17;
    h->stdMode[0] = 1;
    h->stdClut[0] = 0;
    h->stdU0[0] = 0;
    h->stdU1[0] = 31;
    h->stdV0[0] = 7;
    h->stdV1[0] = 7;
    h->stdIL[0] = 0;
    h->stdCount = 1;

    /* Tile 5,16 is the interleaved-only footprint of page 17 mode 1. */
    harness_prefill_tile(h, 5, 16);
    harness_set_phys_only(h, 5, 16, 11);

    r = simulate_standard_lookup(h, 17, 1, 0, 0, 31, 7, 7, 1, &hit);
    expect_count((int)r, (int)VRAM_FRESH_MATERIALIZED);
    expect_count(h->captures, 1);
    expect_count(hit, 1);
    expect_count(h->stdUploads, 0);
    expect_bool(h->stdUsed[0] == 1, 1,
                "same-color mode1 keeps stale entry");
    expect_u64(h->matEpoch[16][5], 11, "same-color mode1 commits epoch");

    hit = 0;
    r = simulate_standard_lookup(h, 17, 1, 0, 0, 31, 7, 7, 1, &hit);
    expect_count((int)r, (int)VRAM_FRESH_NO_ACTION);
    expect_count(h->captures, 1);
    expect_count(hit, 1);
    expect_count(h->stdUploads, 0);
}

static void test_standard_lookup_stale_hit_interleaved_page15_wrap(void)
{
    T6Harness *h = &s_harness;
    VramFreshResult r;
    uint32_t targetBefore, bgBefore;
    unsigned int initialKey;
    int hit = 0;

    harness_reset(h);
    initialKey = (0 & 0x7FFF) |
                 (((uint32_t)production_palette_checksum(
                     h, 0, 1)) << 16);
    h->stdUsed[0] = 1;
    h->stdKey[0] = initialKey;
    h->stdPage[0] = 15;
    h->stdMode[0] = 1;
    h->stdClut[0] = 0;
    h->stdU0[0] = 128;
    h->stdU1[0] = 255;
    h->stdV0[0] = 0;
    h->stdV1[0] = 255;
    h->stdIL[0] = 0;
    h->stdUsed[1] = 1;
    h->stdKey[1] = 0;
    h->stdPage[1] = 14;
    h->stdMode[1] = 1;
    h->stdClut[1] = 0;
    h->stdU0[1] = 0;
    h->stdU1[1] = 255;
    h->stdV0[1] = 0;
    h->stdV1[1] = 255;
    h->stdIL[1] = 0;
    h->stdCount = 2;

    /* Tile 63,0 is the non-wrapped half of page 15 mode 1; the stale entry
     * lives in the wrapped u=128..255 half, so only the full page/mode
     * invalidation can clear it. */
    harness_set_phys_only(h, 63, 0, 11);
    targetBefore = tile_hash(h, 63, 0);
    bgBefore = tile_hash(h, 5, 5);

    r = simulate_standard_lookup(h, 15, 1, 0, 128, 255, 0, 255, 1, &hit);
    expect_count((int)r, (int)VRAM_FRESH_MATERIALIZED);
    expect_count(h->captures, 1);
    expect_count(hit, 0);
    expect_count(h->stdUploads, 1);
    expect_bool(h->stdUsed[0] == 0, 1,
                "page15 wrapped UV stale entry invalidated");
    expect_bool(h->stdUsed[1] == 1, 1,
                "adjacent page14 mode1 entry kept");
    expect_bool(tile_hash(h, 63, 0) != targetBefore, 1,
                "page15 swizzle tile carries GX pixel");
    expect_bool(tile_hash(h, 5, 5) == bgBefore, 1,
                "page15 background unchanged");

    hit = 0;
    r = simulate_standard_lookup(h, 15, 1, 0, 128, 255, 0, 255, 1, &hit);
    expect_count((int)r, (int)VRAM_FRESH_NO_ACTION);
    expect_count(h->captures, 1);
    expect_count(hit, 1);
    expect_count(h->stdUploads, 1);
}

typedef struct T6PartitionIntegrationCtx
{
    T6StandardCacheEntry *store;
    T6StandardCacheEntry *entries;
    unsigned int invalidated[4];
    int invalidateCount;
} T6PartitionIntegrationCtx;

static void integration_entry_reader(
    void *user, int index, unsigned int *clutIdOut, T6StandardPos *posOut)
{
    T6PartitionIntegrationCtx *ctx =
        (T6PartitionIntegrationCtx *)user;
    T6StandardCacheEntry *entry;
    unsigned int packed;

    if (ctx == NULL || clutIdOut == NULL || posOut == NULL)
        return;
    entry = &ctx->entries[index];
    *clutIdOut = entry->clutId;
    packed = entry->packedPos;
    posOut->y2 = (unsigned char)(packed & 0xFF);
    posOut->y1 = (unsigned char)((packed >> 8) & 0xFF);
    posOut->x2 = (unsigned char)((packed >> 16) & 0xFF);
    posOut->x1 = (unsigned char)((packed >> 24) & 0xFF);
}

static void integration_entry_invalidate(void *user, int index)
{
    T6PartitionIntegrationCtx *ctx =
        (T6PartitionIntegrationCtx *)user;

    if (ctx == NULL)
        return;
    ctx->entries[index].clutId = 0;
    ctx->invalidateCount++;
}

static void integration_page_callback(
    void *user, int pageid, int mode, int partition,
    unsigned int packedPos)
{
    T6PartitionIntegrationCtx *ctx =
        (T6PartitionIntegrationCtx *)user;

    (void)pageid;
    (void)mode;
    if (ctx == NULL)
        return;
    ctx->entries = ctx->store + partition * 1024;
    ctx->invalidated[partition]++;
    T6InvalidateStandardPartitionSweep(
        1, packedPos,
        integration_entry_reader, integration_entry_invalidate, ctx);
}

/* Drive the real SOFFA..SOFFD layout through the shared page core and the
 * shared partition sweep with actual T6StandardCacheEntry metadata. */
static void test_standard_partition_sweep_integration(void)
{
    T6StandardCacheEntry store[4 * 1024];
    T6PartitionIntegrationCtx ctx;
    unsigned int clampedNpos = 0x007F00FFu;
    int p;

    memset(store, 0, sizeof(store));
    memset(&ctx, 0, sizeof(ctx));
    ctx.store = store;
    ctx.entries = store;

    store[0 * 1024 + 0].clutId = 1;
    store[0 * 1024 + 0].packedPos =
        T6StandardEntryPackedPos(128, 255, 0, 255);
    store[1 * 1024 + 0].clutId = 1;
    store[1 * 1024 + 0].packedPos =
        T6StandardEntryPackedPos(0, 63, 0, 255);
    store[2 * 1024 + 0].clutId = 0;
    store[2 * 1024 + 0].packedPos =
        T6StandardEntryPackedPos(0, 255, 0, 255);
    store[3 * 1024 + 0].clutId = 1;
    store[3 * 1024 + 0].packedPos =
        T6StandardEntryPackedPos(0, 255, 0, 255);

    T6StandardPageInvalidateCore(15, 1, integration_page_callback, &ctx);
    expect_count(ctx.invalidateCount, 3);
    for (p = 0; p < 4; p++)
        expect_count((int)ctx.invalidated[p], 1);
    expect_bool(store[0 * 1024 + 0].clutId == 0, 1,
                "partition A wrapped entry invalidated");
    expect_bool(store[2 * 1024 + 0].clutId == 0, 1,
                "clutId=0 entry skipped");

    /* A clamped right edge (x2=127) must miss the wrapped u=128..255 entry.
     * Rebuild a fresh fixture because the full-page sweep freed A/B. */
    memset(store, 0, sizeof(store));
    store[0 * 1024 + 0].clutId = 1;
    store[0 * 1024 + 0].packedPos =
        T6StandardEntryPackedPos(128, 255, 0, 255);
    store[1 * 1024 + 0].clutId = 1;
    store[1 * 1024 + 0].packedPos =
        T6StandardEntryPackedPos(0, 63, 0, 255);
    ctx.invalidateCount = 0;
    ctx.entries = store;
    T6InvalidateStandardPartitionSweep(
        1, clampedNpos,
        integration_entry_reader, integration_entry_invalidate, &ctx);
    expect_count(ctx.invalidateCount, 0);

    ctx.invalidateCount = 0;
    ctx.entries = store + 1024;
    T6InvalidateStandardPartitionSweep(
        1, clampedNpos,
        integration_entry_reader, integration_entry_invalidate, &ctx);
    expect_count(ctx.invalidateCount, 1);
}

static void test_read_fresh_early_gate(void)
{
    VramFreshResult r;
    unsigned int sentinel;

    sentinel = 0xDEADBEEFu;
    r = T6ReadFreshEntry(1, 1, 1, 1, &sentinel);
    expect_count((int)r, (int)VRAM_FRESH_NO_ACTION);
    expect_u64((uint64_t)sentinel, 0, "passing entry resets output");

    sentinel = 0xDEADBEEFu;
    r = T6ReadFreshEntry(0, 1, 1, 1, &sentinel);
    expect_count((int)r, (int)VRAM_FRESH_UNRESOLVED);
    expect_u64((uint64_t)sentinel, 0, "disabled resets output");

    sentinel = 0xDEADBEEFu;
    r = T6ReadFreshEntry(1, 0, 1, 1, &sentinel);
    expect_count((int)r, (int)VRAM_FRESH_UNRESOLVED);
    expect_u64((uint64_t)sentinel, 0, "invalid dependency resets output");

    sentinel = 0xDEADBEEFu;
    r = T6ReadFreshEntry(1, 1, 0, 1, &sentinel);
    expect_count((int)r, (int)VRAM_FRESH_UNRESOLVED);
    expect_u64((uint64_t)sentinel, 0, "outer reentrant resets output");

    sentinel = 0xDEADBEEFu;
    r = T6ReadFreshEntry(1, 1, 1, 0, &sentinel);
    expect_count((int)r, (int)VRAM_FRESH_UNRESOLVED);
    expect_u64((uint64_t)sentinel, 0, "inner reentrant resets output");
}

static char *read_whole_file(const char *path)
{
    FILE *f;
    long size;
    char *buf;
    size_t n;

    f = fopen(path, "rb");
    if (f == NULL)
        return NULL;
    if (fseek(f, 0, SEEK_END) != 0)
    {
        fclose(f);
        return NULL;
    }
    size = ftell(f);
    if (fseek(f, 0, SEEK_SET) != 0 || size <= 0)
    {
        fclose(f);
        return NULL;
    }
    buf = (char *)malloc((size_t)size + 1);
    if (buf == NULL)
    {
        fclose(f);
        return NULL;
    }
    n = fread(buf, 1, (size_t)size, f);
    fclose(f);
    if (n != (size_t)size)
    {
        free(buf);
        return NULL;
    }
    buf[n] = '\0';
    return buf;
}

static int count_text_in_range(const char *begin, const char *end,
                               const char *needle)
{
    int count = 0;
    const char *p = begin;

    while (p != NULL && p < end && (p = strstr(p, needle)) != NULL && p < end)
    {
        count++;
        p += strlen(needle);
    }
    return count;
}

/* Minimal production source wiring/mutation check: deleting the
 * SelectSubTextureS() T6InterleavedStandardBarrier() call must fail here. */
static void test_production_wiring_source(void)
{
    static const char *paths[] = {
        "../GlesGpu/gpuTexture.c",
        "WiiSXRX_2022/GlesGpu/gpuTexture.c",
        "GlesGpu/gpuTexture.c"
    };
    int i, opened = 0, wired = 0;

    for (i = 0; i < 3; i++)
    {
        char *buf = read_whole_file(paths[i]);
        char *sel, *next, *call;

        if (buf == NULL)
            continue;
        opened = 1;
        sel = strstr(buf, "GLuint SelectSubTextureS(");
        next = sel != NULL ? strstr(sel, "#endif // _IN_GPU_LIB") : NULL;
        if (sel == NULL)
        {
            free(buf);
            continue;
        }
        if (next == NULL || next < sel)
            next = buf + strlen(buf);
        call = strstr(sel, "T6InterleavedStandardBarrier(");
        if (call != NULL && call < next &&
            strstr(buf, "T6InvalidateStandardPageEntries") != NULL)
            wired = 1;
        free(buf);
        if (wired)
            break;
    }
    expect_bool(opened, 1, "production source opened");
    expect_bool(wired, 1, "SelectSubTextureS wiring present");
}

/* Both production read-fresh entrances must route every early return through
 * the shared T6ReadFreshEntry() stage wrapper (4 calls in gpuVramReadback.inc). */
static void test_read_fresh_entry_wiring(void)
{
    static const char *paths[] = {
        "../GlesGpu/gpuVramReadback.inc",
        "WiiSXRX_2022/GlesGpu/gpuVramReadback.inc",
        "GlesGpu/gpuVramReadback.inc"
    };
    int i, opened = 0, wired = 0;

    for (i = 0; i < 3; i++)
    {
        char *buf = read_whole_file(paths[i]);
        char *materialize, *ensure, *wrapper;

        if (buf == NULL)
            continue;
        opened = 1;
        materialize = strstr(buf,
            "static VramFreshResult MaterializeVramRead(");
        ensure = strstr(buf,
            "static VramFreshResult EnsureVramReadFreshEx(");
        wrapper = strstr(buf,
            "static VramFreshResult EnsureVramReadFresh(");
        if (materialize != NULL && ensure != NULL && wrapper != NULL &&
            materialize < ensure && ensure < wrapper &&
            count_text_in_range(materialize, ensure,
                                "T6ReadFreshEntry(") >= 2 &&
            count_text_in_range(ensure, wrapper,
                                "T6ReadFreshEntry(") >= 2)
            wired = 1;
        free(buf);
        if (wired)
            break;
    }
    expect_bool(opened, 1, "readback source opened");
    expect_bool(wired, 1,
                "both entrances use shared T6ReadFreshEntry early returns");
}

/* P0 regression: MaterializeSnapshotSeq() dereferences the callback user as
 * VramMaterializeSession.  Passing NULL made Wii LTO emit an unconditional
 * trap at the first real materialize commit.  Both DIAG and non-DIAG wiring
 * must pass the live session. */
static void test_materialize_commit_context_wiring(void)
{
    static const char *paths[] = {
        "../GlesGpu/gpuVramReadback.inc",
        "WiiSXRX_2022/GlesGpu/gpuVramReadback.inc",
        "GlesGpu/gpuVramReadback.inc"
    };
    int i, opened = 0, wired = 0;

    for (i = 0; i < 3; i++)
    {
        char *buf = read_whole_file(paths[i]);
        char *core, *coreEnd;

        if (buf == NULL)
            continue;
        opened = 1;
        core = strstr(buf, "static VramFreshResult MaterializeVramReadCore(");
        coreEnd = core != NULL ? strstr(
            core + 1, "static VramFreshResult MaterializeVramRead(") : NULL;
        if (core != NULL && coreEnd != NULL &&
            count_text_in_range(core, coreEnd,
                                "T6MaterializeSessionCommitEpoch(") == 2 &&
            count_text_in_range(core, coreEnd,
                                "ApplyMaterializeEpoch, &session)") == 2 &&
            count_text_in_range(core, coreEnd,
                                "ApplyMaterializeEpoch, NULL)") == 0)
            wired = 1;
        free(buf);
        if (wired)
            break;
    }

    expect_bool(opened, 1, "readback source opened for commit context");
    expect_bool(wired, 1,
                "materialize commit passes session to both callbacks");
}

static void test_snapshot_potential_wiring(void)
{
    static const char *paths[] = {
        "../GlesGpu/gpuVramReadback.inc",
        "WiiSXRX_2022/GlesGpu/gpuVramReadback.inc",
        "GlesGpu/gpuVramReadback.inc"
    };
    int i, opened = 0, wired = 0;

    for (i = 0; i < 3; i++)
    {
        char *buf = read_whole_file(paths[i]);
        char *capture, *captureEnd, *metadata, *merge;
        char *mergeFn, *mergeEnd;

        if (buf == NULL)
            continue;
        opened = 1;
        capture = strstr(buf, "static int PublishCompletedAsyncSnapshot(");
        captureEnd = capture != NULL ?
            strstr(capture + 1, "static BOOL CanCaptureActiveEfb(") : NULL;
        metadata = capture != NULL ?
            strstr(capture, "*target = g_asyncSnapshot;") : NULL;
        merge = metadata != NULL ?
            strstr(metadata, "MergeSelectableSnapshotPotential(target);") : NULL;
        mergeFn = strstr(buf,
            "static void MergeSelectableSnapshotPotential(");
        mergeEnd = mergeFn != NULL ?
            strstr(mergeFn + 1, "static int ContainsPixel(") : NULL;

        if (capture != NULL && captureEnd != NULL && metadata != NULL &&
            merge != NULL && metadata < merge && merge < captureEnd &&
            count_text_in_range(capture, captureEnd,
                                "MergeSelectableSnapshotPotential(target);") == 1 &&
            mergeFn != NULL && mergeEnd != NULL &&
            count_text_in_range(mergeFn, mergeEnd,
                                "T6PotentialMergeSnapshotTile(") == 1 &&
            count_text_in_range(mergeFn, mergeEnd,
                                "snap != &g_snapshot[0]") == 1 &&
            count_text_in_range(mergeFn, mergeEnd,
                                "snap != &g_snapshot[1]") == 1)
            wired = 1;
        free(buf);
        if (wired)
            break;
    }

    expect_bool(opened, 1, "readback source opened for snapshot potential");
    expect_bool(wired, 1,
                "selectable snapshot metadata merges potential before return");
}

static void test_materialize_sample_plan_wiring(void)
{
    static const char *paths[] = {
        "../GlesGpu/gpuVramReadback.inc",
        "WiiSXRX_2022/GlesGpu/gpuVramReadback.inc",
        "GlesGpu/gpuVramReadback.inc"
    };
    int i, opened = 0, wired = 0;

    for (i = 0; i < 3; i++)
    {
        char *buf = read_whole_file(paths[i]);
        char *write, *writeEnd, *build, *current, *baseline;
        char *scalar, *oldRead, *convert, *commitCore;

        if (buf == NULL)
            continue;
        opened = 1;
        write = strstr(buf, "static void WriteMaterializeTile(");
        writeEnd = write != NULL ?
            strstr(write + 1, "static uint64_t MaterializeSnapshotSeq(") :
            NULL;
        build = write != NULL ?
            strstr(write, "T6BuildRGB5A3TileSamplePlan(") : NULL;
        current = build != NULL ?
            strstr(build, "currentRaw = T6ReadRGB5A3TileSample(") : NULL;
        baseline = current != NULL ?
            strstr(current, "baselineRaw = T6ReadRGB5A3TileSample(") : NULL;
        scalar = baseline != NULL ?
            strstr(baseline, "SnapshotPixelCompare(") : NULL;
        oldRead = scalar != NULL ? strstr(scalar, "old = GETLE16(") : NULL;
        convert = oldRead != NULL ?
            strstr(oldRead, "currentRawReady ? GXRGB5A3ToPSX15(currentRaw)") :
            NULL;
        commitCore = writeEnd != NULL ?
            strstr(writeEnd, "T6MaterializeSessionCommitEpoch(") : NULL;

        if (write != NULL && writeEnd != NULL && build != NULL &&
            current != NULL && baseline != NULL && scalar != NULL &&
            oldRead != NULL && convert != NULL &&
            build < current && current < baseline && baseline < scalar &&
            scalar < oldRead && oldRead < convert && convert < writeEnd &&
            count_text_in_range(write, writeEnd,
                                "T6BuildRGB5A3TileSamplePlan(") == 1 &&
            count_text_in_range(write, writeEnd,
                                "T6ReadRGB5A3TileSample(") == 2 &&
            count_text_in_range(write, writeEnd,
                                "ReadSnapshotPixelToPSX(") == 1 &&
            count_text_in_range(write, writeEnd,
                                "GETLE16(") == 1 &&
            commitCore != NULL)
            wired = 1;
        free(buf);
        if (wired)
            break;
    }

    expect_bool(opened, 1,
                "readback source opened for materialize sample plan");
    expect_bool(wired, 1,
                "materialize precomputes sampling, keeps scalar fallback, "
                "delays VRAM read and retains epoch commit");
}

static void test_vram_write_payload_gate_wiring(void)
{
    static const char *pluginPaths[] = {
        "../GlesGpu/gpuPlugin.c",
        "WiiSXRX_2022/GlesGpu/gpuPlugin.c",
        "GlesGpu/gpuPlugin.c"
    };
    static const char *primPaths[] = {
        "../GlesGpu/gpuPrim.c",
        "WiiSXRX_2022/GlesGpu/gpuPrim.c",
        "GlesGpu/gpuPrim.c"
    };
    int i, pluginOpened = 0, pluginWired = 0;
    int primOpened = 0, primWired = 0;

    for (i = 0; i < 3; i++)
    {
        char *buf = read_whole_file(pluginPaths[i]);
        char *writeEntry, *writeEnd, *normalize, *start;

        if (buf == NULL)
            continue;
        pluginOpened = 1;
        writeEntry = strstr(buf, "void CALLBACK GL_GPUwriteDataMem(");
        writeEnd = writeEntry != NULL ?
                   strstr(writeEntry, "void CALLBACK GL_GPUwriteData(") :
                   NULL;
        normalize = writeEntry != NULL ?
                    strstr(writeEntry, "iDataWriteMode=DR_NORMAL;") : NULL;
        start = writeEntry != NULL ? strstr(writeEntry, "STARTVRAM_GL:") :
                NULL;
        if (writeEntry != NULL && writeEnd != NULL &&
            normalize != NULL && start != NULL && normalize < start &&
            count_text_in_range(
                writeEntry, writeEnd,
                "T6VramWritePayloadActive(") == 3 &&
            count_text_in_range(
                writeEntry, writeEnd,
                "T6VramWriteModeNeedsNormalize(") == 1 &&
            count_text_in_range(
                writeEntry, writeEnd,
                "iDataWriteMode==DR_VRAMTRANSFER") == 5 &&
            strstr(buf, "bVramWriteTransferActive = FALSE;") != NULL &&
            strstr(buf, "bVramWriteTransferActive=FALSE;") != NULL &&
            strstr(buf, "TRB A0 GATE call=%u") != NULL)
            pluginWired = 1;
        free(buf);
        if (pluginWired)
            break;
    }

    for (i = 0; i < 3; i++)
    {
        char *buf = read_whole_file(primPaths[i]);
        char *load, *loadEnd;

        if (buf == NULL)
            continue;
        primOpened = 1;
        load = strstr(buf, "static void primLoadImage (");
        loadEnd = load != NULL ? strstr(load + 1, "\nstatic void ") : NULL;
        if (load != NULL && loadEnd != NULL &&
            count_text_in_range(load, loadEnd,
                                "bVramWriteTransferActive = TRUE;") == 1)
            primWired = 1;
        free(buf);
        if (primWired)
            break;
    }

    expect_bool(pluginOpened, 1, "GPU write gate source opened");
    expect_bool(pluginWired, 1,
                "GPU write gate wraps raw and command parser transitions");
    expect_bool(primOpened, 1, "A0 source opened for write gate");
    expect_bool(primWired, 1, "A0 exclusively arms write descriptor");
}

static void test_filtered_resolution_promotion_wiring(void)
{
    static const char *paths[] = {
        "../GlesGpu/gpuVramReadback.inc",
        "WiiSXRX_2022/GlesGpu/gpuVramReadback.inc",
        "GlesGpu/gpuVramReadback.inc"
    };
    int i, opened = 0, wired = 0;

    for (i = 0; i < 3; i++)
    {
        char *buf = read_whole_file(paths[i]);
        char *merge, *mergeEnd, *resolved, *c0Written;
        char *promote, *invalidate;

        if (buf == NULL)
            continue;
        opened = 1;
        merge = strstr(buf, "static void MergeReadbackToPsxVuwFiltered(");
        mergeEnd = merge != NULL ?
                   strstr(merge + 1, "static void MergeReadbackToPsxVuw(") :
                   NULL;
        resolved = merge != NULL ?
                   strstr(merge, "uint16_t filteredResolved") : NULL;
        c0Written = merge != NULL ?
                    strstr(merge, "unsigned int c0Written") : NULL;
        promote = merge != NULL ?
                  strstr(merge, "T6FullTileResolutionPromotionEpoch(") :
                  NULL;
        invalidate = merge != NULL ?
                     strstr(merge, "ForEachHorizontalTileRun(&writtenTile") :
                     NULL;
        if (merge != NULL && mergeEnd != NULL && resolved != NULL &&
            c0Written != NULL &&
            promote != NULL && invalidate != NULL &&
            merge < c0Written && merge < resolved &&
            c0Written < promote && resolved < promote &&
            promote < invalidate && invalidate < mergeEnd &&
            count_text_in_range(
                merge, mergeEnd, "tileCount.filteredResolved[") >= 5 &&
            count_text_in_range(
                merge, mergeEnd, "tileCount.c0Written[") >= 3 &&
            count_text_in_range(
                merge, mergeEnd, "SnapshotPixelCompare(") == 1 &&
            count_text_in_range(
                merge, mergeEnd,
                "SnapshotBaselineResolutionEligible(") == 1 &&
            count_text_in_range(
                merge, mergeEnd,
                "T6FullTileResolutionPromotionEpoch(") == 1 &&
            count_text_in_range(
                merge, mergeEnd,
                "ForEachHorizontalTileRun(&writtenTile") == 1 &&
            count_text_in_range(
                merge, mergeEnd, "memset(&tileCount, 0,") == 1)
            wired = 1;
        free(buf);
        if (wired)
            break;
    }

    expect_bool(opened, 1,
                "readback source opened for resolution promotion");
    expect_bool(wired, 1,
                "filtered full resolution promotes before written-only invalidation");
}

static void harness_move_shadow_validate_reason(
    const T6Harness *h, int ty, int tx, int *reasonOut)
{
    if (h->mapKind[ty][tx] == T6_MAP_UNKNOWN)
        *reasonOut = T6_REASON_UNKNOWN_MAP;
    else if (h->rgb24)
        *reasonOut = T6_REASON_RGB24;
    else if (h->contaminated || h->mixedMapping || h->untrackedEfb)
        *reasonOut = T6_REASON_HAZARD;
    else if (!h->snapshotFull[ty][tx])
        *reasonOut = T6_REASON_PARTIAL;
    else if (!h->snapshotPresent[ty][tx])
        *reasonOut = T6_REASON_CAPTURE_FAIL;
    else if (h->baselinePresent[ty][tx] && !h->baselineUsable[ty][tx])
        *reasonOut = T6_REASON_BASELINE_STALE;
    else
        *reasonOut = T6_REASON_NONE;
}

static VramFreshResult harness_move_shadow(
    T6Harness *h, int x, int y, int w, int height, int vramHeight,
    VramMoveShadowDecision *out)
{
    VramReadDependency dep;
    VramMaterializeSession session;
    T6HarnessRun run;
    int i;

    T6VramMoveShadowDecisionInit(out);
    out->unresolvedReason = T6_REASON_NONE;
    if (w <= 0 || height <= 0)
    {
        out->unresolvedReason = T6_REASON_CAPTURE_FAIL;
        return VRAM_FRESH_UNRESOLVED;
    }
    if (vramHeight != 512)
    {
        out->unresolvedReason = T6_REASON_PARTIAL;
        return VRAM_FRESH_UNRESOLVED;
    }

    memset(&dep, 0, sizeof(dep));
    if (!BuildMoveSourceDependency(x, y, w, height,
                                   1024, vramHeight, &dep))
    {
        out->unresolvedReason = T6_REASON_OVERFLOW;
        return VRAM_FRESH_UNRESOLVED;
    }

    memset(&run, 0, sizeof(run));
    run.h = h;
    run.session = &session;
    T6MaterializeSessionInit(&session, &s_ws);
    ForEachVramReadDependencyTile(&dep, 1024, 512,
                                  harness_observe, &run);

    out->hazardTiles = session.plan.hazardCount;
    out->captureRequired = session.captureRequired;
    if (session.overflow)
    {
        out->unresolvedReason = T6_REASON_OVERFLOW;
        return VRAM_FRESH_UNRESOLVED;
    }
    if (session.captureConflict)
    {
        out->unresolvedReason = T6_REASON_HAZARD;
        return VRAM_FRESH_UNRESOLVED;
    }
    if (session.plan.hazardCount == 0)
    {
        out->result = VRAM_FRESH_NO_ACTION;
        return VRAM_FRESH_NO_ACTION;
    }

    if (session.captureRequired)
    {
        int unknownMap = 0;
        int reason;
        int slot = -1;

        for (i = 0; i < session.touchedCount; i++)
        {
            int idx = session.touched[i];
            int ttx = idx % T6_VRAM_TILE_X;
            int tty = idx / T6_VRAM_TILE_X;

            if (session.plan.hazard[tty][ttx] &&
                h->mapKind[tty][ttx] == T6_MAP_UNKNOWN)
                unknownMap = 1;
        }
        reason = T6ClassifyCaptureFailureReason(
            h->rgb24, h->contaminated, h->mixedMapping,
            h->untrackedEfb, unknownMap);
        if (reason != T6_REASON_CAPTURE_FAIL)
        {
            out->unresolvedReason = reason;
            return VRAM_FRESH_UNRESOLVED;
        }
        /* Mirror production capture plan: safe slot selection plus a
         * capture source that can actually be captured. */
        if (!T6MaterializeSessionSelectCaptureSlot(&session, 0, &slot) ||
            !h->captureSucceed)
        {
            out->unresolvedReason = T6_REASON_CAPTURE_FAIL;
            return VRAM_FRESH_UNRESOLVED;
        }
        out->wouldCapture = 1;
        out->capturePlanSlot = slot;
        out->captureBufferReady = h->captureBufferReady;
        if (!out->captureBufferReady)
        {
            out->unresolvedReason = T6_REASON_CAPTURE_FAIL;
            return VRAM_FRESH_UNRESOLVED;
        }
        for (i = 0; i < session.touchedCount; i++)
        {
            int idx = session.touched[i];
            int ttx = idx % T6_VRAM_TILE_X;
            int tty = idx / T6_VRAM_TILE_X;
            uint64_t requiredSeq;
            uint64_t psxColor;
            uint64_t selectedSeq = 0;
            int selectedFull = 0;
            int reason = T6_REASON_NONE;
            VramTileFreshnessInput in;

            if (!session.plan.hazard[tty][ttx])
                continue;

            requiredSeq =
                T6MaterializeSessionRequiredSeq(&session, ttx, tty);
            psxColor = h->cpuEpoch[tty][ttx] > h->matEpoch[tty][ttx] ?
                       h->cpuEpoch[tty][ttx] : h->matEpoch[tty][ttx];

            /* Existing evidence survives unless it is the planned
             * replacement slot. */
            if (h->snapshotPresent[tty][ttx] &&
                h->snapshotSlot[tty][ttx] != slot &&
                h->snapshotFull[tty][ttx] &&
                h->snapshotSeq[tty][ttx] > psxColor)
            {
                selectedSeq = h->snapshotSeq[tty][ttx];
                selectedFull = 1;
            }

            /* The synthetic capture only owns its own map. */
            if (T6MoveCaptureCandidateMatchesMap(
                    session.captureMapId,
                    h->tileMapId[tty][ttx]) &&
                h->physPresent[tty][ttx] && h->captureFull &&
                h->physSeq[tty][ttx] > psxColor &&
                h->physSeq[tty][ttx] >= selectedSeq)
            {
                selectedSeq = h->physSeq[tty][ttx];
                selectedFull = 1;
            }

            memset(&in, 0, sizeof(in));
            in.cpuWriteEpoch = h->cpuEpoch[tty][ttx];
            in.materializedColorEpoch = h->matEpoch[tty][ttx];
            in.efbSeq = requiredSeq;
            in.snapshotSeq = selectedSeq;
            in.efbCoverFull = selectedFull;
            in.rgb24 = h->rgb24;
            in.contaminated = h->contaminated;
            in.mixedMapping = h->mixedMapping;
            in.untrackedEfb = h->untrackedEfb ||
                h->mapKind[tty][ttx] == T6_MAP_UNKNOWN;
            in.hasValidSnapshot = selectedFull &&
                                  selectedSeq >= requiredSeq;
            in.baselinePresentForSnapshot =
                h->baselinePresent[tty][ttx];
            in.baselineUsable = h->baselineUsable[tty][ttx];

            if (in.rgb24)
                reason = T6_REASON_RGB24;
            else if (in.contaminated || in.mixedMapping ||
                     in.untrackedEfb)
                reason = h->mapKind[tty][ttx] == T6_MAP_UNKNOWN ?
                         T6_REASON_UNKNOWN_MAP : T6_REASON_HAZARD;
            else if (!in.efbCoverFull)
                reason = T6_REASON_PARTIAL;
            else if (!in.hasValidSnapshot)
                reason = T6_REASON_CAPTURE_FAIL;
            else if (in.baselinePresentForSnapshot &&
                     !in.baselineUsable)
                reason = T6_REASON_BASELINE_STALE;

            if (EvaluateVramMaterializeTile(&in) !=
                VRAM_FRESH_MATERIALIZED)
            {
                out->unresolvedReason = reason != T6_REASON_NONE ?
                                        reason : T6_REASON_CAPTURE_FAIL;
                out->unresolvedTileX = ttx;
                out->unresolvedTileY = tty;
                out->unresolvedCpuWriteEpoch = in.cpuWriteEpoch;
                out->unresolvedSnapshotSeq = in.snapshotSeq;
                out->unresolvedRequiredSeq = requiredSeq;
                out->unresolvedMapId =
                    T6MaterializeSessionRequiredMapId(
                        &session, ttx, tty);
                return VRAM_FRESH_UNRESOLVED;
            }
        }
        out->result = VRAM_FRESH_MATERIALIZED;
        return VRAM_FRESH_MATERIALIZED;
    }

    if (!T6MaterializeSessionValidate(&session,
                                      harness_validate, &run))
    {
        int reason = T6_REASON_NONE;

        for (i = 0; i < session.touchedCount &&
                     reason == T6_REASON_NONE; i++)
        {
            int idx = session.touched[i];
            int ttx = idx % T6_VRAM_TILE_X;
            int tty = idx / T6_VRAM_TILE_X;

            if (session.plan.hazard[tty][ttx])
                harness_move_shadow_validate_reason(
                    h, tty, ttx, &reason);
        }
        out->unresolvedReason = reason != T6_REASON_NONE ?
                                reason : T6_REASON_CAPTURE_FAIL;
        return VRAM_FRESH_UNRESOLVED;
    }
    out->result = VRAM_FRESH_MATERIALIZED;
    return VRAM_FRESH_MATERIALIZED;
}

static void test_move_shadow_full_decision(void)
{
    T6Harness *h = &s_harness;
    VramMoveShadowDecision out;
    VramFreshResult r;

    harness_reset(h);
    r = harness_move_shadow(h, 0, 0, 16, 16, 512, &out);
    expect_count((int)r, (int)VRAM_FRESH_NO_ACTION);
    expect_count(out.hazardTiles, 0);
    expect_count(out.captureRequired, 0);
    expect_count(out.capturePlanSlot, -1);
    expect_count(out.unresolvedReason, T6_REASON_NONE);

    harness_reset(h);
    harness_set_tile(h, 0, 0, 11);
    r = harness_move_shadow(h, 0, 0, 16, 16, 512, &out);
    expect_count((int)r, (int)VRAM_FRESH_MATERIALIZED);
    expect_count(out.hazardTiles, 1);
    expect_count(out.wouldCapture, 0);

    harness_reset(h);
    harness_set_phys_only(h, 0, 0, 11);
    r = harness_move_shadow(h, 0, 0, 16, 16, 512, &out);
    expect_count((int)r, (int)VRAM_FRESH_MATERIALIZED);
    expect_count(out.captureRequired, 1);
    expect_count(out.wouldCapture, 1);
    expect_count(out.capturePlanSlot, 0);
    expect_count(out.captureBufferReady, 1);
    expect_count(out.unresolvedReason, T6_REASON_NONE);

    /* A planned capture is not evidence that it can be executed.  The
     * shadow must fail closed until the selected slot already owns a large
     * enough buffer, because it is not allowed to allocate or mutate. */
    harness_reset(h);
    harness_set_phys_only(h, 0, 0, 11);
    h->captureBufferReady = 0;
    r = harness_move_shadow(h, 0, 0, 16, 16, 512, &out);
    expect_count((int)r, (int)VRAM_FRESH_UNRESOLVED);
    expect_count(out.captureRequired, 1);
    expect_count(out.wouldCapture, 1);
    expect_count(out.capturePlanSlot, 0);
    expect_count(out.captureBufferReady, 0);
    expect_count(out.unresolvedReason, T6_REASON_CAPTURE_FAIL);
    expect_count(out.unresolvedTileX, -1);
    expect_count(out.unresolvedTileY, -1);

    harness_reset(h);
    harness_set_phys_only(h, 0, 0, 11);
    h->baselinePresent[0][0] = 1;
    h->baselineUsable[0][0] = 0;
    h->cpuEpoch[0][0] = 2;
    r = harness_move_shadow(h, 0, 0, 16, 16, 512, &out);
    expect_count((int)r, (int)VRAM_FRESH_UNRESOLVED);
    expect_count(out.captureRequired, 1);
    expect_count(out.wouldCapture, 1);
    expect_count(out.capturePlanSlot, 0);
    expect_count(out.unresolvedReason, T6_REASON_BASELINE_STALE);
    expect_count(out.unresolvedTileX, 0);
    expect_count(out.unresolvedTileY, 0);

    harness_reset(h);
    harness_set_phys_only(h, 0, 0, 11);
    h->baselinePresent[0][0] = 1;
    h->baselineUsable[0][0] = 1;
    r = harness_move_shadow(h, 0, 0, 16, 16, 512, &out);
    expect_count((int)r, (int)VRAM_FRESH_MATERIALIZED);
    expect_count(out.captureRequired, 1);
    expect_count(out.wouldCapture, 1);
    expect_count(out.unresolvedReason, T6_REASON_NONE);

    /* Capture eligibility alone is insufficient: post-capture tile
     * metadata must still prove FULL coverage. */
    harness_reset(h);
    harness_set_phys_only(h, 0, 0, 11);
    h->captureFull = 0;
    r = harness_move_shadow(h, 0, 0, 16, 16, 512, &out);
    expect_count((int)r, (int)VRAM_FRESH_UNRESOLVED);
    expect_count(out.captureRequired, 1);
    expect_count(out.wouldCapture, 1);
    expect_count(out.unresolvedReason, T6_REASON_PARTIAL);

    /* Multi-map batch: capture map 1 must not replace or lend baseline/
     * snapshot evidence to the retained full snapshot for map 2. */
    harness_reset(h);
    harness_set_phys_only(h, 0, 0, 11);
    h->tileMapId[0][0] = 1;
    h->mapKind[0][0] = T6_MAP_CURRENT;
    h->snapshotPresent[0][1] = 1;
    h->snapshotFull[0][1] = 1;
    h->snapshotSeq[0][1] = 12;
    h->snapshotSlot[0][1] = 0;
    h->tileMapId[0][1] = 2;
    h->mapKind[0][1] = T6_MAP_PREVIOUS;
    r = harness_move_shadow(h, 0, 0, 32, 16, 512, &out);
    expect_count((int)r, (int)VRAM_FRESH_MATERIALIZED);
    expect_count(out.captureRequired, 1);
    expect_count(out.capturePlanSlot, 1);
    expect_count(out.unresolvedReason, T6_REASON_NONE);

    harness_reset(h);
    h->snapshotPresent[0][0] = 1;
    h->snapshotFull[0][0] = 1;
    h->snapshotSeq[0][0] = 10;
    h->snapshotSlot[0][0] = 0;
    h->physPresent[0][0] = 1;
    h->physSeq[0][0] = 11;
    r = harness_move_shadow(h, 0, 0, 16, 16, 512, &out);
    expect_count((int)r, (int)VRAM_FRESH_MATERIALIZED);
    expect_count(out.captureRequired, 1);
    expect_count(out.wouldCapture, 1);
    expect_count(out.unresolvedReason, T6_REASON_NONE);

    harness_reset(h);
    harness_set_phys_only(h, 0, 0, 11);
    h->cpuEpoch[0][0] = 12;
    r = harness_move_shadow(h, 0, 0, 16, 16, 512, &out);
    expect_count((int)r, (int)VRAM_FRESH_NO_ACTION);
    expect_count(out.hazardTiles, 0);

    harness_reset(h);
    h->snapshotPresent[0][0] = 1;
    h->snapshotFull[0][0] = 0;
    h->snapshotSeq[0][0] = 11;
    h->snapshotSlot[0][0] = 0;
    h->physPresent[0][0] = 1;
    h->physSeq[0][0] = 11;
    r = harness_move_shadow(h, 0, 0, 16, 16, 512, &out);
    expect_count((int)r, (int)VRAM_FRESH_UNRESOLVED);
    expect_count(out.unresolvedReason, T6_REASON_PARTIAL);

    harness_reset(h);
    harness_set_tile(h, 0, 0, 11);
    h->baselinePresent[0][0] = 1;
    h->baselineUsable[0][0] = 0;
    r = harness_move_shadow(h, 0, 0, 16, 16, 512, &out);
    expect_count((int)r, (int)VRAM_FRESH_UNRESOLVED);
    expect_count(out.unresolvedReason, T6_REASON_BASELINE_STALE);

    harness_reset(h);
    harness_set_tile(h, 0, 0, 11);
    h->rgb24 = 1;
    r = harness_move_shadow(h, 0, 0, 16, 16, 512, &out);
    expect_count((int)r, (int)VRAM_FRESH_UNRESOLVED);
    expect_count(out.unresolvedReason, T6_REASON_RGB24);

    harness_reset(h);
    harness_set_tile(h, 0, 0, 11);
    h->mapKind[0][0] = T6_MAP_UNKNOWN;
    r = harness_move_shadow(h, 0, 0, 16, 16, 512, &out);
    expect_count((int)r, (int)VRAM_FRESH_UNRESOLVED);
    expect_count(out.unresolvedReason, T6_REASON_UNKNOWN_MAP);

    harness_reset(h);
    harness_set_tile(h, 63, 31, 11);
    harness_set_tile(h, 0, 31, 11);
    harness_set_tile(h, 63, 0, 11);
    harness_set_tile(h, 0, 0, 11);
    r = harness_move_shadow(h, 1022, 510, 4, 4, 512, &out);
    expect_count((int)r, (int)VRAM_FRESH_MATERIALIZED);
    expect_count(out.hazardTiles, 4);

    /* Two physical hazards on different maps cause capture conflict. */
    harness_reset(h);
    harness_set_phys_only(h, 0, 0, 11);
    harness_set_phys_only(h, 1, 0, 11);
    h->tileMapId[0][1] = 2;
    r = harness_move_shadow(h, 0, 0, 32, 16, 512, &out);
    expect_count((int)r, (int)VRAM_FRESH_UNRESOLVED);
    expect_count(out.unresolvedReason, T6_REASON_HAZARD);

    /* Both snapshot slots are required by the hazard batch, so the capture
     * slot selection fails closed. */
    harness_reset(h);
    h->physPresent[0][0] = 1;
    h->physSeq[0][0] = 12;
    h->snapshotPresent[0][0] = 1;
    h->snapshotFull[0][0] = 1;
    h->snapshotSeq[0][0] = 11;
    h->snapshotSlot[0][0] = 0;
    h->snapshotPresent[0][1] = 1;
    h->snapshotFull[0][1] = 1;
    h->snapshotSeq[0][1] = 11;
    h->snapshotSlot[0][1] = 1;
    h->snapshotPresent[0][2] = 1;
    h->snapshotFull[0][2] = 1;
    h->snapshotSeq[0][2] = 11;
    h->snapshotSlot[0][2] = 0;
    r = harness_move_shadow(h, 0, 0, 48, 16, 512, &out);
    expect_count((int)r, (int)VRAM_FRESH_UNRESOLVED);
    expect_count(out.unresolvedReason, T6_REASON_CAPTURE_FAIL);

    /* 1024-line VRAM Y-wrap is a documented capability limit and fails
     * closed instead of producing a decision. */
    harness_reset(h);
    r = harness_move_shadow(h, 1022, 510, 4, 4, 1024, &out);
    expect_count((int)r, (int)VRAM_FRESH_UNRESOLVED);
    expect_count(out.unresolvedReason, T6_REASON_PARTIAL);
}

static void test_move_shadow_comparator(void)
{
    VramMoveShadowDecision before;
    VramMoveOldPathEvidence after;

    memset(&before, 0, sizeof(before));
    memset(&after, 0, sizeof(after));
    after.evidenceValid = 1;
    after.executed = 1;
    after.mergeCompleted = 1;
    after.succeeded = 1;

    /* MATERIALIZED match requires changed tile/pixel evidence, a source hash
     * change, the same map and at least the required sequence.  The generic
     * expectation is self-consistent: hazardTiles == requiredTiles == 1. */
    before.result = VRAM_FRESH_MATERIALIZED;
    before.hazardTiles = 1;
    before.requiredMapId = 7;
    before.requiredSeq = 11;
    before.sourceHash = 0x1111u;
    before.requiredTile[0][0] = 1;
    before.requiredTileSeq[0][0] = 11;
    before.requiredTileMapId[0][0] = 7;
    before.requiredTiles = 1;
    after.changedTiles = 1;
    after.changedPixels = 100;
    after.changedTileBitmap[0] = 0x1u;
    after.usedTile[0][0] = 1;
    after.usedTileSeq[0][0] = 11;
    after.usedTileMapId[0][0] = 7;
    after.usedTileSourceId[0][0] = 0x5000u;
    after.usedTiles = 1;
    after.requiredMapId = 7;
    after.requiredSeq = 11;
    after.sourceHash = 0x2222u;
    expect_count(T6MoveShadowCompareDecision(&before, &after),
                 T6_MOVE_COMPARE_MATCH);

    /* Incomplete generic evidence: a second hazard that never entered the
     * required bitmap must fail closed instead of matching. */
    before.hazardTiles = 2;
    expect_count(T6MoveShadowCompareDecision(&before, &after),
                 T6_MOVE_COMPARE_INCONCLUSIVE);
    before.hazardTiles = 1;

    /* MATERIALIZED with an empty required set must never match. */
    before.requiredTile[0][0] = 0;
    before.requiredTileSeq[0][0] = 0;
    before.requiredTileMapId[0][0] = 0;
    before.requiredTiles = 0;
    before.hazardTiles = 0;
    expect_count(T6MoveShadowCompareDecision(&before, &after),
                 T6_MOVE_COMPARE_INCONCLUSIVE);
    before.requiredTile[0][0] = 1;
    before.requiredTileSeq[0][0] = 11;
    before.requiredTileMapId[0][0] = 7;
    before.requiredTiles = 1;
    before.hazardTiles = 1;

    /* A required tile without a sequence is unusable evidence. */
    before.requiredTileSeq[0][0] = 0;
    expect_count(T6MoveShadowCompareDecision(&before, &after),
                 T6_MOVE_COMPARE_INCONCLUSIVE);
    before.requiredTileSeq[0][0] = 11;

    /* A coordinate hit without a completed merge is never a match. */
    after.mergeCompleted = 0;
    expect_count(T6MoveShadowCompareDecision(&before, &after),
                 T6_MOVE_COMPARE_MISMATCH);
    after.mergeCompleted = 1;

    /* Changed-range/hash disagreement is a mismatch. */
    after.sourceHash = 0x1111u;
    expect_count(T6MoveShadowCompareDecision(&before, &after),
                 T6_MOVE_COMPARE_MISMATCH);
    after.sourceHash = 0x2222u;
    after.usedTileMapId[0][0] = 8;
    expect_count(T6MoveShadowCompareDecision(&before, &after),
                 T6_MOVE_COMPARE_MISMATCH);
    after.usedTileMapId[0][0] = 7;
    after.usedTileSeq[0][0] = 10;
    expect_count(T6MoveShadowCompareDecision(&before, &after),
                 T6_MOVE_COMPARE_MISMATCH);
    after.usedTileSeq[0][0] = 11;

    /* Source identity 0 must never match: the evidence cannot prove which
     * snapshot produced the tile. */
    after.usedTileSourceId[0][0] = 0;
    expect_count(T6MoveShadowCompareDecision(&before, &after),
                 T6_MOVE_COMPARE_INCONCLUSIVE);
    after.usedTileSourceId[0][0] = 0x5000u;

    /* Missing evidence is inconclusive, even when hazards are gone. */
    after.requiredSeq = 11;
    after.evidenceValid = 0;
    expect_count(T6MoveShadowCompareDecision(&before, &after),
                 T6_MOVE_COMPARE_INCONCLUSIVE);
    after.evidenceValid = 1;

    /* Same-color materialize is a valid epoch-only MATCH when the old path
     * really executed and promoted the freshness epoch but wrote no
     * pixels: succeeded is derived from the promotion. */
    after.changedTileBitmap[0] = 0;
    after.changedTiles = 0;
    after.changedPixels = 0;
    after.promotedTiles = 1;
    after.promotedTileBitmap[0] = 0x1u;
    after.sourceHash = before.sourceHash;
    expect_count(T6MoveShadowCompareDecision(&before, &after),
                 T6_MOVE_COMPARE_MATCH);

    /* A coordinate hit where the old path really did nothing (no pixels,
     * no epoch promotion) is never a match. */
    after.succeeded = 0;
    after.promotedTiles = 0;
    after.promotedTileBitmap[0] = 0;
    after.changedTiles = 0;
    after.changedPixels = 0;
    after.sourceHash = 0x2222u;
    expect_count(T6MoveShadowCompareDecision(&before, &after),
                 T6_MOVE_COMPARE_MISMATCH);
    after.succeeded = 1;

    /* A generic UNRESOLVED must never count as a match, even when the old
     * path clears every hazard afterwards. */
    before.result = VRAM_FRESH_UNRESOLVED;
    before.unresolvedReason = T6_REASON_RGB24;
    after.changedTiles = 0;
    after.changedPixels = 0;
    after.sourceHash = before.sourceHash;
    expect_count(T6MoveShadowCompareDecision(&before, &after),
                 T6_MOVE_COMPARE_INCONCLUSIVE);

    /* NO_ACTION match requires proof the old path wrote nothing and left the
     * source hash unchanged.  Rebuild both fixtures from scratch so no
     * per-tile residue from the MATERIALIZED cases above can leak into the
     * NO_ACTION contract (the relational guard treats stale seq/map on an
     * unused tile as invalid evidence). */
    memset(&before, 0, sizeof(before));
    before.result = VRAM_FRESH_NO_ACTION;
    before.sourceHash = 0x3333u;
    T6VramMoveOldPathEvidenceInit(&after);
    after.evidenceValid = 1;
    after.executed = 1;
    after.mergeCompleted = 1;
    after.sourceHash = before.sourceHash;
    expect_count(T6MoveShadowCompareDecision(&before, &after),
                 T6_MOVE_COMPARE_MATCH);

    /* Promotion-only side effect must not match NO_ACTION.  The promoted
     * tile is recorded as used so the relational contract holds; the
     * NO_ACTION branch must still reject the epoch side effect. */
    T6VramMoveOldPathEvidenceInit(&after);
    after.evidenceValid = 1;
    after.executed = 1;
    after.mergeCompleted = 1;
    after.sourceHash = before.sourceHash;
    after.promotedTiles = 1;
    after.promotedTileBitmap[0] = 0x1u;
    after.invalidateRuns = 1;
    after.succeeded = 1;
    after.usedTile[0][0] = 1;
    after.usedTileSeq[0][0] = 11;
    after.usedTileMapId[0][0] = 7;
    after.usedTileSourceId[0][0] = 0x5000u;
    after.usedTiles = 1;
    expect_count(T6MoveShadowCompareDecision(&before, &after),
                 T6_MOVE_COMPARE_MISMATCH);

    /* Invalidation-only side effect must not match NO_ACTION. */
    T6VramMoveOldPathEvidenceInit(&after);
    after.evidenceValid = 1;
    after.executed = 1;
    after.mergeCompleted = 1;
    after.sourceHash = before.sourceHash;
    after.invalidateRuns = 1;
    expect_count(T6MoveShadowCompareDecision(&before, &after),
                 T6_MOVE_COMPARE_MISMATCH);

    /* CPU-newer + old path tries to merge: changed pixels and hash move.
     * The changed tile is a used tile so the evidence is self-consistent. */
    T6VramMoveOldPathEvidenceInit(&after);
    after.evidenceValid = 1;
    after.executed = 1;
    after.mergeCompleted = 1;
    after.sourceHash = 0x4444u;
    after.changedTiles = 1;
    after.changedPixels = 1;
    after.succeeded = 1;
    after.changedTileBitmap[0] = 0x1u;
    after.usedTile[0][0] = 1;
    after.usedTileSeq[0][0] = 11;
    after.usedTileMapId[0][0] = 7;
    after.usedTileSourceId[0][0] = 0x5000u;
    after.usedTiles = 1;
    expect_count(T6MoveShadowCompareDecision(&before, &after),
                 T6_MOVE_COMPARE_MISMATCH);

    /* NO_ACTION with a used tile but every other side effect zero: the
     * usedTiles gate must reject it. */
    T6VramMoveOldPathEvidenceInit(&after);
    after.evidenceValid = 1;
    after.executed = 1;
    after.mergeCompleted = 1;
    after.sourceHash = before.sourceHash;
    after.usedTile[0][0] = 1;
    after.usedTileSeq[0][0] = 11;
    after.usedTileMapId[0][0] = 7;
    after.usedTileSourceId[0][0] = 0x5000u;
    after.usedTiles = 1;
    expect_count(T6MoveShadowCompareDecision(&before, &after),
                 T6_MOVE_COMPARE_MISMATCH);

    /* NO_ACTION: used bitmap/count disagreement must fail closed instead of
     * trusting the cached count. */
    after.usedTiles = 0;
    expect_count(T6MoveShadowCompareDecision(&before, &after),
                 T6_MOVE_COMPARE_INCONCLUSIVE);

    /* Missing evidence is inconclusive for NO_ACTION too. */
    T6VramMoveOldPathEvidenceInit(&after);
    after.executed = 1;
    after.mergeCompleted = 1;
    after.sourceHash = before.sourceHash;
    expect_count(T6MoveShadowCompareDecision(&before, &after),
                 T6_MOVE_COMPARE_INCONCLUSIVE);
}

static void test_move_shadow_overflow_injection(void)
{
    VramMaterializeSession session;
    VramMaterializeWorkspace ws;
    int i;

    T6MaterializeSessionInit(&session, &ws);
    for (i = 0; i < T6_MAX_TOUCHED_TILES; i++)
        session.touched[i] = (uint16_t)i;
    session.touchedCount = T6_MAX_TOUCHED_TILES;

    T6MaterializeSessionObserveTile(
        &session, 0, 0, 0, 11, 0, 1, -1, T6_MAP_CURRENT, 1, 11);
    expect_count(session.overflow, 1);
    expect_count(session.plan.hazardCount, 0);
}

static void test_move_evidence_tile_count_matches(void)
{
    VramMoveOldPathEvidence ev;

    memset(&ev, 0, sizeof(ev));
    ev.changedTiles = 2;
    ev.changedTileBitmap[0] = 0x3u;
    expect_bool(T6MoveEvidenceTileCountMatches(&ev), 1,
                "bitmap count matches changedTiles");

    ev.changedTileBitmap[0] = 0x1u;
    expect_bool(T6MoveEvidenceTileCountMatches(&ev), 0,
                "bitmap count disagrees");

    ev.changedTiles = 0;
    ev.changedTileBitmap[0] = 0;
    expect_bool(T6MoveEvidenceTileCountMatches(&ev), 1,
                "zero tiles matches empty bitmap");
}

static void test_move_shadow_contract(void)
{
    VramMoveShadowDecision before;
    VramMoveOldPathEvidence after;
    uint64_t seq[T6_VRAM_TILE_Y][T6_VRAM_TILE_X];
    uint64_t usedSeq[T6_VRAM_TILE_Y][T6_VRAM_TILE_X];
    uint64_t usedSourceId[T6_VRAM_TILE_Y][T6_VRAM_TILE_X];
    uint64_t sourceId1d[T6_VRAM_TILE_X * T6_VRAM_TILE_Y];
    unsigned char used[T6_VRAM_TILE_Y][T6_VRAM_TILE_X];
    unsigned char mixed[T6_VRAM_TILE_Y][T6_VRAM_TILE_X];
    unsigned int mapId[T6_VRAM_TILE_Y][T6_VRAM_TILE_X];
    unsigned int usedMapId[T6_VRAM_TILE_Y][T6_VRAM_TILE_X];
    unsigned int zeroChanged[64] = {0};
    unsigned int zeroPromoted[64] = {0};
    unsigned int oneChanged[64] = {0};
    unsigned int onePromoted[64] = {0};
    unsigned int changedB[64] = {0};
    unsigned int promotedB[64] = {0};
    int ok;

    memset(&before, 0, sizeof(before));
    T6VramMoveOldPathEvidenceInit(&after);
    expect_count(after.executed, 0);
    expect_count(after.mergeCompleted, 0);
    expect_count(after.evidenceValid, 0);

    oneChanged[0] = 0x1u;
    onePromoted[0] = 0x1u;
    changedB[0] = 0x2u;   /* tile (0,1), the only tile used in case 2 */
    promotedB[0] = 0x2u;

    /* Required hazard set: tiles A(0,0) seq=5 map=2, B(1,0) seq=10 map=2. */
    before.result = VRAM_FRESH_MATERIALIZED;
    before.hazardTiles = 2;
    before.requiredTile[0][0] = 1;
    before.requiredTileSeq[0][0] = 5;
    before.requiredTileMapId[0][0] = 2;
    before.requiredTile[0][1] = 1;
    before.requiredTileSeq[0][1] = 10;
    before.requiredTileMapId[0][1] = 2;
    before.requiredTiles = 2;
    before.sourceHash = 0x1111u;
    after.executed = 1;

    /* Case 1: both required tiles covered by same map and sufficient seq. */
    memset(used, 0, sizeof(used));
    memset(mixed, 0, sizeof(mixed));
    memset(usedSeq, 0, sizeof(usedSeq));
    memset(usedSourceId, 0, sizeof(usedSourceId));
    memset(usedMapId, 0, sizeof(usedMapId));
    used[0][0] = 1; usedSeq[0][0] = 6; usedMapId[0][0] = 2;
    usedSourceId[0][0] = 101;
    used[0][1] = 1; usedSeq[0][1] = 10; usedMapId[0][1] = 2;
    usedSourceId[0][1] = 102;
    ok = T6MoveEvidenceFinalize(
        &after, 100u, 1u, oneChanged, 1u, onePromoted,
        2u, 0x2222u, 2u, 10u,
        used, mixed, usedSeq, usedSourceId, usedMapId);
    expect_bool(ok, 1, "full coverage finalize");
    expect_count(after.usedTiles, 2);
    expect_count(T6MoveShadowCompareDecision(&before, &after),
                 T6_MOVE_COMPARE_MATCH);

    /* Case 2: only max-seq tile B covered -> missing hazard tile A. */
    memset(used, 0, sizeof(used));
    memset(mixed, 0, sizeof(mixed));
    memset(usedSeq, 0, sizeof(usedSeq));
    memset(usedSourceId, 0, sizeof(usedSourceId));
    memset(usedMapId, 0, sizeof(usedMapId));
    used[0][1] = 1; usedSeq[0][1] = 10; usedMapId[0][1] = 2;
    usedSourceId[0][1] = 102;
    ok = T6MoveEvidenceFinalize(
        &after, 100u, 1u, changedB, 1u, promotedB,
        2u, 0x2222u, 2u, 10u,
        used, mixed, usedSeq, usedSourceId, usedMapId);
    expect_bool(ok, 1, "partial coverage finalize");
    expect_count(after.usedTiles, 1);
    expect_count(T6MoveShadowCompareDecision(&before, &after),
                 T6_MOVE_COMPARE_MISMATCH);

    /* Case 3: coordinates complete but tile B actual seq < required. */
    memset(used, 0, sizeof(used));
    memset(mixed, 0, sizeof(mixed));
    memset(usedSeq, 0, sizeof(usedSeq));
    memset(usedSourceId, 0, sizeof(usedSourceId));
    memset(usedMapId, 0, sizeof(usedMapId));
    used[0][0] = 1; usedSeq[0][0] = 6; usedMapId[0][0] = 2;
    usedSourceId[0][0] = 101;
    used[0][1] = 1; usedSeq[0][1] = 9; usedMapId[0][1] = 2;
    usedSourceId[0][1] = 102;
    ok = T6MoveEvidenceFinalize(
        &after, 100u, 1u, oneChanged, 1u, onePromoted,
        2u, 0x2222u, 2u, 10u,
        used, mixed, usedSeq, usedSourceId, usedMapId);
    expect_bool(ok, 1, "low seq finalize");
    expect_count(T6MoveShadowCompareDecision(&before, &after),
                 T6_MOVE_COMPARE_MISMATCH);

    /* Case 4: mixed tile -> inconclusive. */
    memset(used, 0, sizeof(used));
    memset(mixed, 0, sizeof(mixed));
    memset(usedSeq, 0, sizeof(usedSeq));
    memset(usedSourceId, 0, sizeof(usedSourceId));
    memset(usedMapId, 0, sizeof(usedMapId));
    used[0][0] = 1; usedSeq[0][0] = 6; usedMapId[0][0] = 2;
    usedSourceId[0][0] = 101;
    mixed[0][0] = 1;
    used[0][1] = 1; usedSeq[0][1] = 10; usedMapId[0][1] = 2;
    usedSourceId[0][1] = 102;
    ok = T6MoveEvidenceFinalize(
        &after, 100u, 1u, oneChanged, 1u, onePromoted,
        2u, 0x2222u, 2u, 10u,
        used, mixed, usedSeq, usedSourceId, usedMapId);
    expect_bool(ok, 1, "mixed tile finalize");
    expect_count(T6MoveShadowCompareDecision(&before, &after),
                 T6_MOVE_COMPARE_INCONCLUSIVE);

    /* Collector: identical seq/map/sourceId observations are not mixed. */
    memset(sourceId1d, 0, sizeof(sourceId1d));
    memset(seq, 0, sizeof(seq));
    memset(used, 0, sizeof(used));
    memset(mixed, 0, sizeof(mixed));
    memset(mapId, 0, sizeof(mapId));
    T6MoveRecordUsedTile((unsigned char *)used, (uint64_t *)seq,
                         (unsigned int *)mapId, (unsigned char *)mixed,
                         sourceId1d, 0, 10, 2, 101);
    T6MoveRecordUsedTile((unsigned char *)used, (uint64_t *)seq,
                         (unsigned int *)mapId, (unsigned char *)mixed,
                         sourceId1d, 0, 10, 2, 101);
    expect_bool(mixed[0][0] == 0, 1, "identical observation not mixed");
    expect_u64(seq[0][0], 10, "identical observation keeps seq");
    expect_u64(sourceId1d[0], 101, "identical observation keeps source");

    /* Collector: same slot, different sourceId must be mixed even when
     * seq/map are identical (slot reuse, new capture). */
    memset(sourceId1d, 0, sizeof(sourceId1d));
    memset(seq, 0, sizeof(seq));
    memset(used, 0, sizeof(used));
    memset(mixed, 0, sizeof(mixed));
    memset(mapId, 0, sizeof(mapId));
    T6MoveRecordUsedTile((unsigned char *)used, (uint64_t *)seq,
                         (unsigned int *)mapId, (unsigned char *)mixed,
                         sourceId1d, 0, 10, 2, 101);
    T6MoveRecordUsedTile((unsigned char *)used, (uint64_t *)seq,
                         (unsigned int *)mapId, (unsigned char *)mixed,
                         sourceId1d, 0, 10, 2, 102);
    expect_bool(mixed[0][0] == 1, 1, "slot reuse with new source mixed");
    expect_u64(seq[0][0], 10, "slot reuse keeps seq");

    /* Collector: same sourceId/map, seq 5 then 10 keeps max seq and still
     * marks mixed because the observation changed. */
    memset(sourceId1d, 0, sizeof(sourceId1d));
    memset(seq, 0, sizeof(seq));
    memset(used, 0, sizeof(used));
    memset(mixed, 0, sizeof(mixed));
    memset(mapId, 0, sizeof(mapId));
    T6MoveRecordUsedTile((unsigned char *)used, (uint64_t *)seq,
                         (unsigned int *)mapId, (unsigned char *)mixed,
                         sourceId1d, 0, 5, 2, 101);
    T6MoveRecordUsedTile((unsigned char *)used, (uint64_t *)seq,
                         (unsigned int *)mapId, (unsigned char *)mixed,
                         sourceId1d, 0, 10, 2, 101);
    expect_u64(seq[0][0], 10, "same tile keeps max seq");
    expect_bool(mixed[0][0] == 1, 1, "seq change marked mixed");

    /* Collector -> finalize: different sourceId in the same slot must never
     * MATCH (mixed evidence stays INCONCLUSIVE). */
    memset(used, 0, sizeof(used));
    memset(mixed, 0, sizeof(mixed));
    memset(usedSeq, 0, sizeof(usedSeq));
    memset(usedSourceId, 0, sizeof(usedSourceId));
    memset(usedMapId, 0, sizeof(usedMapId));
    used[0][0] = 1; usedSeq[0][0] = 10; usedMapId[0][0] = 2;
    mixed[0][0] = 1;
    usedSourceId[0][0] = 102;
    ok = T6MoveEvidenceFinalize(
        &after, 100u, 1u, oneChanged, 1u, onePromoted,
        2u, 0x2222u, 2u, 10u,
        used, mixed, usedSeq, usedSourceId, usedMapId);
    expect_bool(ok, 1, "slot-reuse finalize");
    expect_count(T6MoveShadowCompareDecision(&before, &after),
                 T6_MOVE_COMPARE_INCONCLUSIVE);

    /* Collector -> finalize: sourceId 0 must never MATCH. */
    memset(used, 0, sizeof(used));
    memset(mixed, 0, sizeof(mixed));
    memset(usedSeq, 0, sizeof(usedSeq));
    memset(usedSourceId, 0, sizeof(usedSourceId));
    memset(usedMapId, 0, sizeof(usedMapId));
    used[0][0] = 1; usedSeq[0][0] = 10; usedMapId[0][0] = 2;
    ok = T6MoveEvidenceFinalize(
        &after, 100u, 1u, oneChanged, 1u, onePromoted,
        2u, 0x2222u, 2u, 10u,
        used, mixed, usedSeq, usedSourceId, usedMapId);
    expect_bool(ok, 1, "unknown source finalize");
    expect_count(T6MoveShadowCompareDecision(&before, &after),
                 T6_MOVE_COMPARE_INCONCLUSIVE);

    /* NO_ACTION with zero side effects. */
    memset(&before, 0, sizeof(before));
    before.result = VRAM_FRESH_NO_ACTION;
    before.sourceHash = 0x1111u;
    memset(used, 0, sizeof(used));
    memset(mixed, 0, sizeof(mixed));
    memset(usedSeq, 0, sizeof(usedSeq));
    memset(usedSourceId, 0, sizeof(usedSourceId));
    memset(usedMapId, 0, sizeof(usedMapId));
    ok = T6MoveEvidenceFinalize(
        &after, 0u, 0u, zeroChanged, 0u, zeroPromoted,
        0u, 0x1111u, 1u, 0u,
        used, mixed, usedSeq, usedSourceId, usedMapId);
    expect_bool(ok, 1, "noaction finalize");
    expect_count(after.succeeded, 0);
    expect_count(T6MoveShadowCompareDecision(&before, &after),
                 T6_MOVE_COMPARE_MATCH);

    /* NO_ACTION with a used tile but zero side effects must not match. */
    memset(used, 0, sizeof(used));
    memset(mixed, 0, sizeof(mixed));
    memset(usedSeq, 0, sizeof(usedSeq));
    memset(usedSourceId, 0, sizeof(usedSourceId));
    memset(usedMapId, 0, sizeof(usedMapId));
    used[0][0] = 1; usedSeq[0][0] = 5; usedMapId[0][0] = 2;
    usedSourceId[0][0] = 101;
    ok = T6MoveEvidenceFinalize(
        &after, 0u, 0u, zeroChanged, 0u, zeroPromoted,
        0u, 0x1111u, 1u, 0u,
        used, mixed, usedSeq, usedSourceId, usedMapId);
    expect_bool(ok, 1, "noaction used tile finalize");
    expect_count(T6MoveShadowCompareDecision(&before, &after),
                 T6_MOVE_COMPARE_MISMATCH);

    /* Promotion-only side effect must not match NO_ACTION.  The promoted
     * tile is recorded as used so the relational contract holds; the
     * NO_ACTION branch must reject the epoch promotion. */
    memset(used, 0, sizeof(used));
    memset(mixed, 0, sizeof(mixed));
    memset(usedSeq, 0, sizeof(usedSeq));
    memset(usedSourceId, 0, sizeof(usedSourceId));
    memset(usedMapId, 0, sizeof(usedMapId));
    used[0][0] = 1; usedSeq[0][0] = 5; usedMapId[0][0] = 2;
    usedSourceId[0][0] = 101;
    ok = T6MoveEvidenceFinalize(
        &after, 0u, 0u, zeroChanged, 1u, onePromoted,
        1u, 0x1111u, 1u, 0u,
        used, mixed, usedSeq, usedSourceId, usedMapId);
    expect_bool(ok, 1, "promotion-only finalize");
    expect_count(T6MoveShadowCompareDecision(&before, &after),
                 T6_MOVE_COMPARE_MISMATCH);

    /* Invalidation-only side effect must not match NO_ACTION. */
    memset(used, 0, sizeof(used));
    memset(mixed, 0, sizeof(mixed));
    memset(usedSeq, 0, sizeof(usedSeq));
    memset(usedSourceId, 0, sizeof(usedSourceId));
    memset(usedMapId, 0, sizeof(usedMapId));
    ok = T6MoveEvidenceFinalize(
        &after, 0u, 0u, zeroChanged, 0u, zeroPromoted,
        1u, 0x1111u, 1u, 0u,
        used, mixed, usedSeq, usedSourceId, usedMapId);
    expect_bool(ok, 1, "invalidation-only finalize");
    expect_count(T6MoveShadowCompareDecision(&before, &after),
                 T6_MOVE_COMPARE_MISMATCH);

    /* Inconsistent bitmap/count must invalidate evidence. */
    memset(used, 0, sizeof(used));
    memset(mixed, 0, sizeof(mixed));
    memset(usedSeq, 0, sizeof(usedSeq));
    memset(usedSourceId, 0, sizeof(usedSourceId));
    memset(usedMapId, 0, sizeof(usedMapId));
    ok = T6MoveEvidenceFinalize(
        &after, 100u, 2u, oneChanged, 0u, zeroPromoted,
        0u, 0x2222u, 1u, 5u,
        used, mixed, usedSeq, usedSourceId, usedMapId);
    expect_bool(ok, 0, "inconsistent count rejected");
    expect_count(after.evidenceValid, 0);
    expect_count(T6MoveShadowCompareDecision(&before, &after),
                 T6_MOVE_COMPARE_INCONCLUSIVE);

    /* Early-return evidence (executed but never merged) is reported as
     * NOT_MERGED by the single decision entry, not as a generic
     * AFTER_EVIDENCE.  before is a fully-specified MATERIALIZED shadow so
     * the required-evidence gate is passed and NOT_MERGED is reached. */
    memset(&before, 0, sizeof(before));
    before.result = VRAM_FRESH_MATERIALIZED;
    before.hazardTiles = 1;
    before.requiredTile[0][0] = 1;
    before.requiredTileSeq[0][0] = 11;
    before.requiredTileMapId[0][0] = 7;
    before.requiredTiles = 1;
    before.sourceHash = 0x1111u;
    after.executed = 1;
    after.mergeCompleted = 0;
    after.evidenceValid = 0;
    expect_count(T6MoveShadowCompareDecision(&before, &after),
                 T6_MOVE_COMPARE_MISMATCH);
}

static void test_move_compare_detail_kinds(void)
{
    VramMoveShadowDecision before;
    VramMoveOldPathEvidence after;
    T6MoveCompareDetail detail;

    memset(&before, 0, sizeof(before));
    before.result = VRAM_FRESH_MATERIALIZED;
    before.requiredTile[0][0] = 1;
    before.requiredTileSeq[0][0] = 11;
    before.requiredTileMapId[0][0] = 7;
    before.requiredTiles = 1;

    /* ABSENT: required tile never touched. */
    memset(&after, 0, sizeof(after));
    T6MoveCompareDetailScanTiles(&before, &after, &detail);
    expect_count((int)detail.kind, (int)T6_MOVE_BAD_ABSENT);
    expect_count(detail.tx, 0);
    expect_count(detail.ty, 0);
    expect_count((int)detail.missingTiles, 1);

    /* SOURCE_UNKNOWN: used but no snapshot identity. */
    memset(&after, 0, sizeof(after));
    after.usedTile[0][0] = 1;
    after.usedTiles = 1;
    after.usedTileSeq[0][0] = 11;
    after.usedTileMapId[0][0] = 7;
    T6MoveCompareDetailScanTiles(&before, &after, &detail);
    expect_count((int)detail.kind, (int)T6_MOVE_BAD_SOURCE_UNKNOWN);
    expect_count(detail.tx, 0);
    expect_count(detail.ty, 0);

    /* MIXED: snapshot identity changed for the same tile. */
    memset(&after, 0, sizeof(after));
    after.usedTile[0][0] = 1;
    after.usedTiles = 1;
    after.usedTileSeq[0][0] = 11;
    after.usedTileMapId[0][0] = 7;
    after.usedTileSourceId[0][0] = 101;
    after.mixedTile[0][0] = 1;
    T6MoveCompareDetailScanTiles(&before, &after, &detail);
    expect_count((int)detail.kind, (int)T6_MOVE_BAD_MIXED);
    expect_count(detail.tx, 0);
    expect_count(detail.ty, 0);
    expect_count((int)detail.mixedTiles, 1);

    /* MIXED totals are independent of the first-bad priority: a used tile
     * with sourceId=0 AND mixed must keep SOURCE_UNKNOWN priority while
     * mixedTiles still counts the mixed tile. */
    memset(&after, 0, sizeof(after));
    after.usedTile[0][0] = 1;
    after.usedTiles = 1;
    after.usedTileSeq[0][0] = 11;
    after.usedTileMapId[0][0] = 7;
    after.mixedTile[0][0] = 1; /* sourceId stays 0 */
    T6MoveCompareDetailScanTiles(&before, &after, &detail);
    expect_count((int)detail.kind, (int)T6_MOVE_BAD_SOURCE_UNKNOWN);
    expect_count((int)detail.mixedTiles, 1);

    /* MIXED totals on an extra tile: EXTRA keeps first-bad priority and
     * mixedTiles still counts the mixed used tile. */
    memset(&after, 0, sizeof(after));
    after.usedTile[0][0] = 1;
    after.usedTileSeq[0][0] = 11;
    after.usedTileMapId[0][0] = 7;
    after.usedTileSourceId[0][0] = 101;
    after.usedTile[0][1] = 1;
    after.usedTileSeq[0][1] = 12;
    after.usedTileMapId[0][1] = 7;
    after.usedTileSourceId[0][1] = 102;
    after.mixedTile[0][1] = 1;
    after.usedTiles = 2;
    T6MoveCompareDetailScanTiles(&before, &after, &detail);
    expect_count((int)detail.kind, (int)T6_MOVE_BAD_EXTRA);
    expect_count(detail.tx, 1);
    expect_count(detail.ty, 0);
    expect_count((int)detail.extraTiles, 1);
    expect_count((int)detail.mixedTiles, 1);

    /* MAP: same seq but different map id. */
    memset(&after, 0, sizeof(after));
    after.usedTile[0][0] = 1;
    after.usedTiles = 1;
    after.usedTileSeq[0][0] = 11;
    after.usedTileMapId[0][0] = 8;
    after.usedTileSourceId[0][0] = 101;
    T6MoveCompareDetailScanTiles(&before, &after, &detail);
    expect_count((int)detail.kind, (int)T6_MOVE_BAD_MAP);
    expect_count(detail.tx, 0);
    expect_count(detail.ty, 0);
    expect_count((int)detail.requiredMapId, 7);
    expect_count((int)detail.actualMapId, 8);

    /* SEQ: map matches but actual seq is stale. */
    memset(&after, 0, sizeof(after));
    after.usedTile[0][0] = 1;
    after.usedTiles = 1;
    after.usedTileSeq[0][0] = 10;
    after.usedTileMapId[0][0] = 7;
    after.usedTileSourceId[0][0] = 101;
    T6MoveCompareDetailScanTiles(&before, &after, &detail);
    expect_count((int)detail.kind, (int)T6_MOVE_BAD_SEQ);
    expect_count(detail.tx, 0);
    expect_count(detail.ty, 0);
    expect_u64(detail.requiredSeq, 11, "seq detail required");
    expect_u64(detail.actualSeq, 10, "seq detail actual");

    /* EXTRA: required tile satisfied, unexpected tile also used. */
    memset(&after, 0, sizeof(after));
    after.usedTile[0][0] = 1;
    after.usedTileSeq[0][0] = 11;
    after.usedTileMapId[0][0] = 7;
    after.usedTileSourceId[0][0] = 101;
    after.usedTile[0][1] = 1;
    after.usedTileSeq[0][1] = 11;
    after.usedTileMapId[0][1] = 7;
    after.usedTileSourceId[0][1] = 102;
    after.usedTiles = 2;
    T6MoveCompareDetailScanTiles(&before, &after, &detail);
    expect_count((int)detail.kind, (int)T6_MOVE_BAD_EXTRA);
    expect_count(detail.tx, 1);
    expect_count(detail.ty, 0);
    expect_count((int)detail.extraTiles, 1);

    /* USED_COUNT: bitmap and cached count disagree. */
    memset(&after, 0, sizeof(after));
    after.usedTile[0][0] = 1;
    after.usedTileSeq[0][0] = 11;
    after.usedTileMapId[0][0] = 7;
    after.usedTileSourceId[0][0] = 101;
    after.usedTiles = 0;
    T6MoveCompareDetailScanTiles(&before, &after, &detail);
    expect_count((int)detail.kind, (int)T6_MOVE_BAD_USED_COUNT);
    expect_count(detail.tx, 0);
    expect_count(detail.ty, 0);

    /* BAD_BEFORE_EVIDENCE: inconsistent generic expectation must be
     * INCONCLUSIVE with a classifiable reason. */
    memset(&before, 0, sizeof(before));
    before.result = VRAM_FRESH_MATERIALIZED;
    before.hazardTiles = 2;
    before.requiredTile[0][0] = 1;
    before.requiredTileSeq[0][0] = 11;
    before.requiredTileMapId[0][0] = 7;
    before.requiredTiles = 1;
    before.sourceHash = 0x1111u;
    memset(&after, 0, sizeof(after));
    after.evidenceValid = 1;
    after.executed = 1;
    after.mergeCompleted = 1;
    after.succeeded = 1;
    after.usedTile[0][0] = 1;
    after.usedTileSeq[0][0] = 11;
    after.usedTileMapId[0][0] = 7;
    after.usedTileSourceId[0][0] = 101;
    after.usedTiles = 1;
    after.changedTiles = 1;
    after.changedPixels = 1;
    after.changedTileBitmap[0] = 0x1u;
    after.sourceHash = 0x2222u;
    expect_count(T6MoveShadowCompareDecisionEx(&before, &after, &detail),
                 T6_MOVE_COMPARE_INCONCLUSIVE);
    expect_count((int)detail.kind, (int)T6_MOVE_BAD_BEFORE_EVIDENCE);

    /* BAD_CHANGED_OUTSIDE_USED: changed bitmap on a tile the old path
     * never used. */
    memset(&before, 0, sizeof(before));
    before.result = VRAM_FRESH_MATERIALIZED;
    before.hazardTiles = 1;
    before.requiredTile[0][0] = 1;
    before.requiredTileSeq[0][0] = 11;
    before.requiredTileMapId[0][0] = 7;
    before.requiredTiles = 1;
    before.sourceHash = 0x1111u;
    memset(&after, 0, sizeof(after));
    after.evidenceValid = 1;
    after.executed = 1;
    after.mergeCompleted = 1;
    after.succeeded = 1;
    after.usedTile[0][0] = 1;
    after.usedTileSeq[0][0] = 11;
    after.usedTileMapId[0][0] = 7;
    after.usedTileSourceId[0][0] = 101;
    after.usedTiles = 1;
    after.changedTiles = 1;
    after.changedPixels = 1;
    after.changedTileBitmap[0] = 0x2u; /* tile (0,1), not used */
    after.sourceHash = 0x2222u;
    expect_count(T6MoveShadowCompareDecisionEx(&before, &after, &detail),
                 T6_MOVE_COMPARE_INCONCLUSIVE);
    expect_count((int)detail.kind,
                 (int)T6_MOVE_BAD_CHANGED_OUTSIDE_USED);

    /* BAD_PROMOTED_OUTSIDE_USED. */
    memset(&after, 0, sizeof(after));
    after.evidenceValid = 1;
    after.executed = 1;
    after.mergeCompleted = 1;
    after.succeeded = 1;
    after.usedTile[0][0] = 1;
    after.usedTileSeq[0][0] = 11;
    after.usedTileMapId[0][0] = 7;
    after.usedTileSourceId[0][0] = 101;
    after.usedTiles = 1;
    after.promotedTiles = 1;
    after.promotedTileBitmap[0] = 0x2u;
    after.sourceHash = 0x2222u;
    expect_count(T6MoveShadowCompareDecisionEx(&before, &after, &detail),
                 T6_MOVE_COMPARE_INCONCLUSIVE);
    expect_count((int)detail.kind,
                 (int)T6_MOVE_BAD_PROMOTED_OUTSIDE_USED);

    /* BAD_AFTER_EVIDENCE: mixed tile outside the used set. */
    memset(&after, 0, sizeof(after));
    after.evidenceValid = 1;
    after.executed = 1;
    after.mergeCompleted = 1;
    after.succeeded = 1;
    after.usedTile[0][0] = 1;
    after.usedTileSeq[0][0] = 11;
    after.usedTileMapId[0][0] = 7;
    after.usedTileSourceId[0][0] = 101;
    after.usedTiles = 1;
    after.mixedTile[0][1] = 1;
    after.changedTiles = 1;
    after.changedPixels = 1;
    after.changedTileBitmap[0] = 0x1u;
    after.sourceHash = 0x2222u;
    expect_count(T6MoveShadowCompareDecisionEx(&before, &after, &detail),
                 T6_MOVE_COMPARE_INCONCLUSIVE);
    expect_count((int)detail.kind, (int)T6_MOVE_BAD_AFTER_EVIDENCE);

    /* BAD_NOT_EXECUTED / BAD_NOT_MERGED. */
    memset(&after, 0, sizeof(after));
    after.evidenceValid = 1;
    after.executed = 0;
    after.mergeCompleted = 1;
    after.succeeded = 1;
    after.usedTile[0][0] = 1;
    after.usedTileSeq[0][0] = 11;
    after.usedTileMapId[0][0] = 7;
    after.usedTileSourceId[0][0] = 101;
    after.usedTiles = 1;
    after.changedTiles = 1;
    after.changedPixels = 1;
    after.changedTileBitmap[0] = 0x1u;
    after.sourceHash = 0x2222u;
    expect_count(T6MoveShadowCompareDecisionEx(&before, &after, &detail),
                 T6_MOVE_COMPARE_MISMATCH);
    expect_count((int)detail.kind, (int)T6_MOVE_BAD_NOT_EXECUTED);
    after.executed = 1;
    after.mergeCompleted = 0;
    expect_count(T6MoveShadowCompareDecisionEx(&before, &after, &detail),
                 T6_MOVE_COMPARE_MISMATCH);
    expect_count((int)detail.kind, (int)T6_MOVE_BAD_NOT_MERGED);
    after.mergeCompleted = 1;

    /* Production early-return shape: the merge never started so the
     * evidence was never finalized (evidenceValid=0).  NOT_MERGED must win
     * over the generic AFTER_EVIDENCE fallback. */
    memset(&after, 0, sizeof(after));
    after.executed = 1;
    after.mergeCompleted = 0;
    after.evidenceValid = 0;
    expect_count(T6MoveShadowCompareDecisionEx(&before, &after, &detail),
                 T6_MOVE_COMPARE_MISMATCH);
    expect_count((int)detail.kind, (int)T6_MOVE_BAD_NOT_MERGED);

    /* BAD_NOT_SUCCEEDED: MATERIALIZED, required tile (0,0) covered with
     * the exact expected sequence (no ABSENT/MIXED/MAP/SEQ), but the old
     * path really did nothing (no pixels, no epoch promotion).  The
     * evidence is self-consistent, so the relational gate passes and the
     * business comparison must reject it. */
    after.mergeCompleted = 1;
    after.evidenceValid = 1;
    after.usedTile[0][0] = 1;
    after.usedTileSeq[0][0] = 11;
    after.usedTileMapId[0][0] = 7;
    after.usedTileSourceId[0][0] = 101;
    after.usedTiles = 1;
    after.changedTiles = 0;
    after.changedPixels = 0;
    after.changedTileBitmap[0] = 0;
    after.promotedTiles = 0;
    after.promotedTileBitmap[0] = 0;
    after.succeeded = 0;
    expect_count(T6MoveShadowCompareDecisionEx(&before, &after, &detail),
                 T6_MOVE_COMPARE_MISMATCH);
    expect_count((int)detail.kind, (int)T6_MOVE_BAD_NOT_SUCCEEDED);
    after.succeeded = 1;
    after.changedTiles = 1;
    after.changedPixels = 1;
    after.changedTileBitmap[0] = 0x1u;

    /* BAD_HASH: changed tiles but the source hash never moved. */
    after.sourceHash = before.sourceHash;
    expect_count(T6MoveShadowCompareDecisionEx(&before, &after, &detail),
                 T6_MOVE_COMPARE_MISMATCH);
    expect_count((int)detail.kind, (int)T6_MOVE_BAD_HASH);
    after.sourceHash = 0x2222u;

    /* NO_ACTION clean match and its side-effect kinds. */
    memset(&before, 0, sizeof(before));
    before.result = VRAM_FRESH_NO_ACTION;
    before.sourceHash = 0x1111u;
    memset(&after, 0, sizeof(after));
    after.evidenceValid = 1;
    after.executed = 1;
    after.mergeCompleted = 1;
    after.sourceHash = 0x1111u;
    expect_count(T6MoveShadowCompareDecisionEx(&before, &after, &detail),
                 T6_MOVE_COMPARE_MATCH);
    expect_count((int)detail.kind, (int)T6_MOVE_BAD_NONE);

    after.usedTile[0][0] = 1;
    after.usedTileSeq[0][0] = 11;
    after.usedTileMapId[0][0] = 7;
    after.usedTileSourceId[0][0] = 101;
    after.usedTiles = 1;
    expect_count(T6MoveShadowCompareDecisionEx(&before, &after, &detail),
                 T6_MOVE_COMPARE_MISMATCH);
    expect_count((int)detail.kind, (int)T6_MOVE_BAD_NOACTION_USED);
    after.usedTile[0][0] = 0;
    after.usedTileSeq[0][0] = 0;
    after.usedTileMapId[0][0] = 0;
    after.usedTileSourceId[0][0] = 0;
    after.usedTiles = 0;

    after.invalidateRuns = 1;
    expect_count(T6MoveShadowCompareDecisionEx(&before, &after, &detail),
                 T6_MOVE_COMPARE_MISMATCH);
    expect_count((int)detail.kind,
                 (int)T6_MOVE_BAD_NOACTION_INVALIDATED);
    after.invalidateRuns = 0;

    after.sourceHash = 0x2222u;
    expect_count(T6MoveShadowCompareDecisionEx(&before, &after, &detail),
                 T6_MOVE_COMPARE_MISMATCH);
    expect_count((int)detail.kind, (int)T6_MOVE_BAD_HASH);
    after.sourceHash = 0x1111u;

    /* NOACTION_CHANGED: a changed tile recorded as used (relational holds)
     * must be reported as the changed side effect, not just USED. */
    memset(&after, 0, sizeof(after));
    after.evidenceValid = 1;
    after.executed = 1;
    after.mergeCompleted = 1;
    after.sourceHash = 0x1111u;
    after.changedTiles = 1;
    after.changedPixels = 1;
    after.succeeded = 1;
    after.changedTileBitmap[0] = 0x1u;
    after.usedTile[0][0] = 1;
    after.usedTileSeq[0][0] = 11;
    after.usedTileMapId[0][0] = 7;
    after.usedTileSourceId[0][0] = 101;
    after.usedTiles = 1;
    expect_count(T6MoveShadowCompareDecisionEx(&before, &after, &detail),
                 T6_MOVE_COMPARE_MISMATCH);
    expect_count((int)detail.kind, (int)T6_MOVE_BAD_NOACTION_CHANGED);

    /* NOACTION_PROMOTED: promotion recorded on a used tile. */
    memset(&after, 0, sizeof(after));
    after.evidenceValid = 1;
    after.executed = 1;
    after.mergeCompleted = 1;
    after.sourceHash = 0x1111u;
    after.promotedTiles = 1;
    after.promotedTileBitmap[0] = 0x1u;
    after.succeeded = 1;
    after.usedTile[0][0] = 1;
    after.usedTileSeq[0][0] = 11;
    after.usedTileMapId[0][0] = 7;
    after.usedTileSourceId[0][0] = 101;
    after.usedTiles = 1;
    expect_count(T6MoveShadowCompareDecisionEx(&before, &after, &detail),
                 T6_MOVE_COMPARE_MISMATCH);
    expect_count((int)detail.kind, (int)T6_MOVE_BAD_NOACTION_PROMOTED);

    /* BAD_ARGUMENT: NULL inputs are INCONCLUSIVE with a non-NONE reason. */
    expect_count(T6MoveShadowCompareDecisionEx(NULL, &after, &detail),
                 T6_MOVE_COMPARE_INCONCLUSIVE);
    expect_count((int)detail.kind, (int)T6_MOVE_BAD_ARGUMENT);
    expect_count(T6MoveShadowCompareDecisionEx(&before, NULL, &detail),
                 T6_MOVE_COMPARE_INCONCLUSIVE);
    expect_count((int)detail.kind, (int)T6_MOVE_BAD_ARGUMENT);

    /* BAD_BEFORE_UNRESOLVED: unreachable freshness must be named, not left
     * as BAD_NONE. */
    memset(&before, 0, sizeof(before));
    before.result = VRAM_FRESH_UNRESOLVED;
    before.unresolvedReason = T6_REASON_RGB24;
    expect_count(T6MoveShadowCompareDecisionEx(&before, &after, &detail),
                 T6_MOVE_COMPARE_INCONCLUSIVE);
    expect_count((int)detail.kind,
                 (int)T6_MOVE_BAD_BEFORE_UNRESOLVED);
}

static void test_move_required_consistency(void)
{
    VramMoveShadowDecision before;
    VramMoveOldPathEvidence after;

    memset(&before, 0, sizeof(before));
    memset(&after, 0, sizeof(after));
    before.result = VRAM_FRESH_MATERIALIZED;
    before.hazardTiles = 1;
    before.requiredTile[0][0] = 1;
    before.requiredTileSeq[0][0] = 11;
    before.requiredTileMapId[0][0] = 7;
    before.requiredTiles = 1;
    before.sourceHash = 0x1111u;
    after.evidenceValid = 1;
    after.executed = 1;
    after.mergeCompleted = 1;
    after.succeeded = 1;
    after.usedTile[0][0] = 1;
    after.usedTileSeq[0][0] = 11;
    after.usedTileMapId[0][0] = 7;
    after.usedTileSourceId[0][0] = 101;
    after.usedTiles = 1;
    after.changedTiles = 1;
    after.changedPixels = 1;
    after.changedTileBitmap[0] = 0x1u;
    after.sourceHash = 0x2222u;
    expect_count(T6MoveShadowCompareDecision(&before, &after),
                 T6_MOVE_COMPARE_MATCH);

    /* 1. hazardTiles=2 but required bitmap/count=1: a generic hazard
     * missing from the required set must never MATCH. */
    before.hazardTiles = 2;
    expect_count(T6MoveShadowCompareDecision(&before, &after),
                 T6_MOVE_COMPARE_INCONCLUSIVE);
    before.hazardTiles = 1;

    /* 2. MATERIALIZED with an empty required set, after claims success. */
    before.requiredTile[0][0] = 0;
    before.requiredTileSeq[0][0] = 0;
    before.requiredTileMapId[0][0] = 0;
    before.requiredTiles = 0;
    before.hazardTiles = 0;
    expect_count(T6MoveShadowCompareDecision(&before, &after),
                 T6_MOVE_COMPARE_INCONCLUSIVE);

    /* 3. required bitmap popcount=2 but requiredTiles=1. */
    before.requiredTile[0][0] = 1;
    before.requiredTileSeq[0][0] = 11;
    before.requiredTileMapId[0][0] = 7;
    before.requiredTile[0][1] = 1;
    before.requiredTileSeq[0][1] = 11;
    before.requiredTileMapId[0][1] = 7;
    before.requiredTiles = 1;
    before.hazardTiles = 2;
    expect_count(T6MoveShadowCompareDecision(&before, &after),
                 T6_MOVE_COMPARE_INCONCLUSIVE);

    /* 4. bitmap/count=1 but hazardTiles=0. */
    before.requiredTile[0][1] = 0;
    before.requiredTileSeq[0][1] = 0;
    before.requiredTileMapId[0][1] = 0;
    before.requiredTiles = 1;
    before.hazardTiles = 0;
    expect_count(T6MoveShadowCompareDecision(&before, &after),
                 T6_MOVE_COMPARE_INCONCLUSIVE);
    before.hazardTiles = 1;

    /* 5. NO_ACTION with required evidence or hazards is INCONCLUSIVE; a
     * clean NO_ACTION with identical hash and zero side effects matches. */
    before.result = VRAM_FRESH_NO_ACTION;
    before.sourceHash = 0x1111u;
    after.changedTiles = 0;
    after.changedPixels = 0;
    after.changedTileBitmap[0] = 0;
    after.succeeded = 0;
    after.sourceHash = 0x1111u;
    expect_count(T6MoveShadowCompareDecision(&before, &after),
                 T6_MOVE_COMPARE_INCONCLUSIVE);
    before.requiredTile[0][0] = 0;
    before.requiredTileSeq[0][0] = 0;
    before.requiredTileMapId[0][0] = 0;
    before.requiredTiles = 0;
    before.hazardTiles = 0;
    after.usedTile[0][0] = 0;
    after.usedTileSeq[0][0] = 0;
    after.usedTileMapId[0][0] = 0;
    after.usedTileSourceId[0][0] = 0;
    after.usedTiles = 0;
    expect_count(T6MoveShadowCompareDecision(&before, &after),
                 T6_MOVE_COMPARE_MATCH);

    /* 6. A required tile without a sequence is unusable evidence. */
    before.result = VRAM_FRESH_MATERIALIZED;
    before.requiredTile[0][0] = 1;
    before.requiredTileSeq[0][0] = 0;
    before.requiredTileMapId[0][0] = 7;
    before.requiredTiles = 1;
    before.hazardTiles = 1;
    after.succeeded = 1;
    after.changedTiles = 1;
    after.changedPixels = 1;
    after.changedTileBitmap[0] = 0x1u;
    after.sourceHash = 0x2222u;
    after.usedTile[0][0] = 1;
    after.usedTileSeq[0][0] = 11;
    after.usedTileMapId[0][0] = 7;
    after.usedTileSourceId[0][0] = 101;
    after.usedTiles = 1;
    expect_count(T6MoveShadowCompareDecision(&before, &after),
                 T6_MOVE_COMPARE_INCONCLUSIVE);
}

/* 95.2: required map identity must be provable.  A required tile with
 * map id 0 is not valid evidence even when the actual map is also 0, and
 * non-required tiles must not carry stale per-tile expectations. */
static void test_move_required_mapid_gate(void)
{
    VramMoveShadowDecision before;
    VramMoveOldPathEvidence after;
    T6MoveCompareDetail detail;

    memset(&before, 0, sizeof(before));
    memset(&after, 0, sizeof(after));
    before.result = VRAM_FRESH_MATERIALIZED;
    before.hazardTiles = 1;
    before.requiredTile[0][0] = 1;
    before.requiredTileSeq[0][0] = 11;
    before.requiredTileMapId[0][0] = 7;
    before.requiredTiles = 1;
    before.sourceHash = 0x1111u;
    after.evidenceValid = 1;
    after.executed = 1;
    after.mergeCompleted = 1;
    after.succeeded = 1;
    after.usedTile[0][0] = 1;
    after.usedTileSeq[0][0] = 11;
    after.usedTileMapId[0][0] = 7;
    after.usedTileSourceId[0][0] = 101;
    after.usedTiles = 1;
    after.changedTiles = 1;
    after.changedPixels = 1;
    after.changedTileBitmap[0] = 0x1u;
    after.sourceHash = 0x2222u;

    /* 95.2-4 baseline: legal non-zero seq/map MATCHes. */
    expect_count(T6MoveShadowCompareDecisionEx(&before, &after, &detail),
                 T6_MOVE_COMPARE_MATCH);
    expect_count((int)detail.kind, (int)T6_MOVE_BAD_NONE);

    /* 95.2-1: required map id 0 with a matching actual map of 0 must be
     * INCONCLUSIVE / BAD_BEFORE_EVIDENCE, never MATCH. */
    before.requiredTileMapId[0][0] = 0;
    after.usedTileMapId[0][0] = 0;
    expect_count(T6MoveShadowCompareDecisionEx(&before, &after, &detail),
                 T6_MOVE_COMPARE_INCONCLUSIVE);
    expect_count((int)detail.kind, (int)T6_MOVE_BAD_BEFORE_EVIDENCE);
    before.requiredTileMapId[0][0] = 7;
    after.usedTileMapId[0][0] = 7;
    expect_count(T6MoveShadowCompareDecisionEx(&before, &after, &detail),
                 T6_MOVE_COMPARE_MATCH);

    /* 95.2-2: requiredTile=0 with stale non-zero seq is rejected. */
    before.requiredTile[0][1] = 0;
    before.requiredTileSeq[0][1] = 5;
    expect_count(T6MoveShadowCompareDecisionEx(&before, &after, &detail),
                 T6_MOVE_COMPARE_INCONCLUSIVE);
    expect_count((int)detail.kind, (int)T6_MOVE_BAD_BEFORE_EVIDENCE);
    before.requiredTileSeq[0][1] = 0;

    /* 95.2-3: requiredTile=0 with stale non-zero map is rejected. */
    before.requiredTileMapId[0][1] = 3;
    expect_count(T6MoveShadowCompareDecisionEx(&before, &after, &detail),
                 T6_MOVE_COMPARE_INCONCLUSIVE);
    expect_count((int)detail.kind, (int)T6_MOVE_BAD_BEFORE_EVIDENCE);
    before.requiredTileMapId[0][1] = 0;

    /* Clean fixture still MATCHes. */
    expect_count(T6MoveShadowCompareDecisionEx(&before, &after, &detail),
                 T6_MOVE_COMPARE_MATCH);
}

/* 96.1-3/4: USED_COUNT must come out of the full decision entry with the
 * independent scan totals intact, and an illegal VramFreshResult must get
 * its own badKind instead of a bare BAD_NONE exit. */
static void test_move_decision_count_result_gate(void)
{
    VramMoveShadowDecision before;
    VramMoveOldPathEvidence after;
    T6MoveCompareDetail detail;

    memset(&before, 0, sizeof(before));
    memset(&after, 0, sizeof(after));
    before.result = VRAM_FRESH_MATERIALIZED;
    before.hazardTiles = 1;
    before.requiredTile[0][0] = 1;
    before.requiredTileSeq[0][0] = 11;
    before.requiredTileMapId[0][0] = 7;
    before.requiredTiles = 1;
    before.sourceHash = 0x1111u;
    after.evidenceValid = 1;
    after.executed = 1;
    after.mergeCompleted = 1;
    after.succeeded = 1;
    /* One used tile outside the required set, but the cached count lies. */
    after.usedTile[0][1] = 1;
    after.usedTileSeq[0][1] = 11;
    after.usedTileMapId[0][1] = 7;
    after.usedTileSourceId[0][1] = 101;
    after.usedTiles = 0;
    after.changedTiles = 1;
    after.changedPixels = 1;
    after.changedTileBitmap[0] = 0x2u;
    after.sourceHash = 0x2222u;

    /* USED_COUNT via the full decision entry, with totals preserved:
     * required tile (0,0) missing, tile (0,1) extra. */
    expect_count(T6MoveShadowCompareDecisionEx(&before, &after, &detail),
                 T6_MOVE_COMPARE_INCONCLUSIVE);
    expect_count((int)detail.kind, (int)T6_MOVE_BAD_USED_COUNT);
    expect_count(detail.tx, 1);
    expect_count(detail.ty, 0);
    expect_count((int)detail.missingTiles, 1);
    expect_count((int)detail.extraTiles, 1);
    expect_count((int)detail.mixedTiles, 0);

    /* Illegal VramFreshResult must be classified, not a bare BAD_NONE. */
    before.result = (VramFreshResult)99;
    expect_count(T6MoveShadowCompareDecisionEx(&before, &after, &detail),
                 T6_MOVE_COMPARE_INCONCLUSIVE);
    expect_count((int)detail.kind, (int)T6_MOVE_BAD_RESULT);
}

static void test_move_evidence_relational(void)
{
    VramMoveShadowDecision before;
    VramMoveOldPathEvidence after;
    unsigned int changedB[64] = {0};

    memset(&before, 0, sizeof(before));
    before.result = VRAM_FRESH_MATERIALIZED;
    before.hazardTiles = 1;
    before.requiredTile[0][0] = 1;
    before.requiredTileSeq[0][0] = 11;
    before.requiredTileMapId[0][0] = 7;
    before.requiredTiles = 1;
    before.sourceHash = 0x1111u;
    changedB[0] = 0x2u; /* tile (0,1) */

    /* 1. used=A, changed bitmap=B: changed outside used -> invalid. */
    memset(&after, 0, sizeof(after));
    after.evidenceValid = 1;
    after.executed = 1;
    after.mergeCompleted = 1;
    after.succeeded = 1;
    after.usedTile[0][0] = 1;
    after.usedTileSeq[0][0] = 11;
    after.usedTileMapId[0][0] = 7;
    after.usedTileSourceId[0][0] = 101;
    after.usedTiles = 1;
    after.changedTiles = 1;
    after.changedPixels = 1;
    memcpy(after.changedTileBitmap, changedB, sizeof(changedB));
    after.sourceHash = 0x2222u;
    expect_count(T6MoveShadowCompareDecision(&before, &after),
                 T6_MOVE_COMPARE_INCONCLUSIVE);

    /* 2. used=A, promoted bitmap=B -> invalid. */
    memset(&after, 0, sizeof(after));
    after.evidenceValid = 1;
    after.executed = 1;
    after.mergeCompleted = 1;
    after.succeeded = 1;
    after.usedTile[0][0] = 1;
    after.usedTileSeq[0][0] = 11;
    after.usedTileMapId[0][0] = 7;
    after.usedTileSourceId[0][0] = 101;
    after.usedTiles = 1;
    after.promotedTiles = 1;
    memcpy(after.promotedTileBitmap, changedB, sizeof(changedB));
    after.sourceHash = 0x2222u;
    expect_count(T6MoveShadowCompareDecision(&before, &after),
                 T6_MOVE_COMPARE_INCONCLUSIVE);

    /* 3. changedPixels=1 but no changed tile/count -> invalid. */
    memset(&after, 0, sizeof(after));
    after.evidenceValid = 1;
    after.executed = 1;
    after.mergeCompleted = 1;
    after.succeeded = 1;
    after.usedTile[0][0] = 1;
    after.usedTileSeq[0][0] = 11;
    after.usedTileMapId[0][0] = 7;
    after.usedTileSourceId[0][0] = 101;
    after.usedTiles = 1;
    after.changedTiles = 0;
    after.changedPixels = 1;
    after.sourceHash = 0x2222u;
    expect_count(T6MoveShadowCompareDecision(&before, &after),
                 T6_MOVE_COMPARE_INCONCLUSIVE);

    /* 4. changed count=1/bitmap=A but changedPixels=0 -> invalid. */
    memset(&after, 0, sizeof(after));
    after.evidenceValid = 1;
    after.executed = 1;
    after.mergeCompleted = 1;
    after.succeeded = 1;
    after.usedTile[0][0] = 1;
    after.usedTileSeq[0][0] = 11;
    after.usedTileMapId[0][0] = 7;
    after.usedTileSourceId[0][0] = 101;
    after.usedTiles = 1;
    after.changedTiles = 1;
    after.changedPixels = 0;
    after.changedTileBitmap[0] = 0x1u;
    after.sourceHash = 0x2222u;
    expect_count(T6MoveShadowCompareDecision(&before, &after),
                 T6_MOVE_COMPARE_INCONCLUSIVE);

    /* 5. mixedTile set on an unused tile -> invalid. */
    memset(&after, 0, sizeof(after));
    after.evidenceValid = 1;
    after.executed = 1;
    after.mergeCompleted = 1;
    after.succeeded = 1;
    after.usedTile[0][0] = 1;
    after.usedTileSeq[0][0] = 11;
    after.usedTileMapId[0][0] = 7;
    after.usedTileSourceId[0][0] = 101;
    after.usedTiles = 1;
    after.mixedTile[0][1] = 1;
    after.changedTiles = 1;
    after.changedPixels = 1;
    after.changedTileBitmap[0] = 0x1u;
    after.sourceHash = 0x2222u;
    expect_count(T6MoveShadowCompareDecision(&before, &after),
                 T6_MOVE_COMPARE_INCONCLUSIVE);

    /* 6. succeeded tampered after finalize: derived value disagrees. */
    memset(&after, 0, sizeof(after));
    after.evidenceValid = 1;
    after.executed = 1;
    after.mergeCompleted = 1;
    after.succeeded = 0; /* should be 1 given changedTiles */
    after.usedTile[0][0] = 1;
    after.usedTileSeq[0][0] = 11;
    after.usedTileMapId[0][0] = 7;
    after.usedTileSourceId[0][0] = 101;
    after.usedTiles = 1;
    after.changedTiles = 1;
    after.changedPixels = 1;
    after.changedTileBitmap[0] = 0x1u;
    after.sourceHash = 0x2222u;
    expect_count(T6MoveShadowCompareDecision(&before, &after),
                 T6_MOVE_COMPARE_INCONCLUSIVE);

    /* 7. used=A with changed/promoted on A stays a valid MATCH. */
    memset(&after, 0, sizeof(after));
    after.evidenceValid = 1;
    after.executed = 1;
    after.mergeCompleted = 1;
    after.succeeded = 1;
    after.usedTile[0][0] = 1;
    after.usedTileSeq[0][0] = 11;
    after.usedTileMapId[0][0] = 7;
    after.usedTileSourceId[0][0] = 101;
    after.usedTiles = 1;
    after.changedTiles = 1;
    after.changedPixels = 1;
    after.changedTileBitmap[0] = 0x1u;
    after.promotedTiles = 1;
    after.promotedTileBitmap[0] = 0x1u;
    after.sourceHash = 0x2222u;
    expect_count(T6MoveShadowCompareDecision(&before, &after),
                 T6_MOVE_COMPARE_MATCH);
}

static void test_move_finalize_end_to_end(void)
{
    VramMoveShadowDecision before;
    VramMoveOldPathEvidence after;
    T6MoveCompareDetail detail;
    unsigned char used[T6_VRAM_TILE_Y][T6_VRAM_TILE_X];
    unsigned char mixed[T6_VRAM_TILE_Y][T6_VRAM_TILE_X];
    uint64_t seq[T6_VRAM_TILE_Y][T6_VRAM_TILE_X];
    uint64_t source[T6_VRAM_TILE_Y][T6_VRAM_TILE_X];
    unsigned int map[T6_VRAM_TILE_Y][T6_VRAM_TILE_X];
    unsigned int changedBm[64];
    unsigned int promotedBm[64];
    int ok;

    memset(&before, 0, sizeof(before));
    before.result = VRAM_FRESH_MATERIALIZED;
    before.hazardTiles = 1;
    before.requiredTile[0][0] = 1;
    before.requiredTileSeq[0][0] = 11;
    before.requiredTileMapId[0][0] = 7;
    before.requiredTiles = 1;
    before.sourceHash = 0x1111u;

    /* End-to-end: finalize rejects changed outside used and the single
     * decision entry reports the precise kind plus the tile coordinates,
     * without hand-filling evidenceValid. */
    memset(used, 0, sizeof(used));
    memset(mixed, 0, sizeof(mixed));
    memset(seq, 0, sizeof(seq));
    memset(source, 0, sizeof(source));
    memset(map, 0, sizeof(map));
    memset(changedBm, 0, sizeof(changedBm));
    memset(promotedBm, 0, sizeof(promotedBm));
    used[0][0] = 1;
    seq[0][0] = 11;
    map[0][0] = 7;
    source[0][0] = 101;
    changedBm[0] = 0x2u; /* tile (0,1): used by nobody */
    T6VramMoveOldPathEvidenceInit(&after);
    after.executed = 1;
    ok = T6MoveEvidenceFinalize(&after, 1, 1, changedBm, 0, promotedBm,
                                0, 0x2222u, 7, 11, used, mixed, seq,
                                source, map);
    expect_count(ok, 0);
    expect_count(after.evidenceValid, 0);
    expect_count((int)after.evidenceErrorKind,
                 (int)T6_MOVE_BAD_CHANGED_OUTSIDE_USED);
    expect_count(after.evidenceErrorTx, 1);
    expect_count(after.evidenceErrorTy, 0);
    expect_count(T6MoveShadowCompareDecisionEx(&before, &after, &detail),
                 T6_MOVE_COMPARE_INCONCLUSIVE);
    expect_count((int)detail.kind,
                 (int)T6_MOVE_BAD_CHANGED_OUTSIDE_USED);
    expect_count(detail.tx, 1);
    expect_count(detail.ty, 0);

    /* End-to-end: promoted outside used. */
    memset(changedBm, 0, sizeof(changedBm));
    promotedBm[0] = 0x4u; /* tile (0,2): used by nobody */
    T6VramMoveOldPathEvidenceInit(&after);
    after.executed = 1;
    ok = T6MoveEvidenceFinalize(&after, 0, 0, changedBm, 1, promotedBm,
                                0, 0x2222u, 7, 11, used, mixed, seq,
                                source, map);
    expect_count(ok, 0);
    expect_count((int)after.evidenceErrorKind,
                 (int)T6_MOVE_BAD_PROMOTED_OUTSIDE_USED);
    expect_count(after.evidenceErrorTx, 2);
    expect_count(after.evidenceErrorTy, 0);
    expect_count(T6MoveShadowCompareDecisionEx(&before, &after, &detail),
                 T6_MOVE_COMPARE_INCONCLUSIVE);
    expect_count((int)detail.kind,
                 (int)T6_MOVE_BAD_PROMOTED_OUTSIDE_USED);
    expect_count(detail.tx, 2);
    expect_count(detail.ty, 0);

    /* End-to-end: stale per-tile residue on an unused tile is classified
     * as generic AFTER_EVIDENCE with the offending tile coordinates. */
    memset(seq, 0, sizeof(seq));
    seq[0][3] = 5; /* unused tile carries a sequence */
    memset(changedBm, 0, sizeof(changedBm));
    memset(promotedBm, 0, sizeof(promotedBm));
    T6VramMoveOldPathEvidenceInit(&after);
    after.executed = 1;
    ok = T6MoveEvidenceFinalize(&after, 0, 0, changedBm, 0, promotedBm,
                                0, 0x2222u, 7, 11, used, mixed, seq,
                                source, map);
    expect_count(ok, 0);
    expect_count((int)after.evidenceErrorKind,
                 (int)T6_MOVE_BAD_AFTER_EVIDENCE);
    expect_count(after.evidenceErrorTx, 3);
    expect_count(after.evidenceErrorTy, 0);
    expect_count(T6MoveShadowCompareDecisionEx(&before, &after, &detail),
                 T6_MOVE_COMPARE_INCONCLUSIVE);
    expect_count((int)detail.kind, (int)T6_MOVE_BAD_AFTER_EVIDENCE);
    expect_count(detail.tx, 3);
    expect_count(detail.ty, 0);

    /* Finalize NULL array guards: every array argument is checked before
     * any memcpy; a NULL leaves evidenceValid=0 and returns 0. */
    memset(seq, 0, sizeof(seq));
    memset(changedBm, 0, sizeof(changedBm));
    memset(promotedBm, 0, sizeof(promotedBm));
    T6VramMoveOldPathEvidenceInit(&after);
    after.executed = 1;
    ok = T6MoveEvidenceFinalize(&after, 0, 0, NULL, 0, promotedBm,
                                0, 0x2222u, 7, 11, used, mixed, seq,
                                source, map);
    expect_count(ok, 0);
    expect_count(after.evidenceValid, 0);
    ok = T6MoveEvidenceFinalize(&after, 0, 0, changedBm, 0, NULL,
                                0, 0x2222u, 7, 11, used, mixed, seq,
                                source, map);
    expect_count(ok, 0);
    expect_count(after.evidenceValid, 0);
    ok = T6MoveEvidenceFinalize(&after, 0, 0, changedBm, 0, promotedBm,
                                0, 0x2222u, 7, 11, NULL, mixed, seq,
                                source, map);
    expect_count(ok, 0);
    expect_count(after.evidenceValid, 0);
    ok = T6MoveEvidenceFinalize(&after, 0, 0, changedBm, 0, promotedBm,
                                0, 0x2222u, 7, 11, used, NULL, seq,
                                source, map);
    expect_count(ok, 0);
    expect_count(after.evidenceValid, 0);
    ok = T6MoveEvidenceFinalize(&after, 0, 0, changedBm, 0, promotedBm,
                                0, 0x2222u, 7, 11, used, mixed, NULL,
                                source, map);
    expect_count(ok, 0);
    expect_count(after.evidenceValid, 0);
    ok = T6MoveEvidenceFinalize(&after, 0, 0, changedBm, 0, promotedBm,
                                0, 0x2222u, 7, 11, used, mixed, seq,
                                NULL, map);
    expect_count(ok, 0);
    expect_count(after.evidenceValid, 0);
    ok = T6MoveEvidenceFinalize(&after, 0, 0, changedBm, 0, promotedBm,
                                0, 0x2222u, 7, 11, used, mixed, seq,
                                source, NULL);
    expect_count(ok, 0);
    expect_count(after.evidenceValid, 0);
    expect_count(T6MoveEvidenceFinalize(NULL, 0, 0, changedBm, 0,
                                        promotedBm, 0, 0x2222u, 7, 11,
                                        used, mixed, seq, source, map), 0);
}

/* Runtime DC2 evidence contained a stable 60-required/59-used boundary
 * mismatch.  A full tile with zero delta is still safely resolved, while a
 * tile for which fewer than 256 pixels were classified must remain absent. */
static void test_move_full_tile_resolution(void)
{
    VramMoveShadowDecision before;
    VramMoveOldPathEvidence after;
    T6MoveCompareDetail detail;
    unsigned char used[T6_VRAM_TILE_Y][T6_VRAM_TILE_X];
    unsigned char mixed[T6_VRAM_TILE_Y][T6_VRAM_TILE_X];
    uint64_t seq[T6_VRAM_TILE_Y][T6_VRAM_TILE_X];
    uint64_t source[T6_VRAM_TILE_Y][T6_VRAM_TILE_X];
    unsigned int map[T6_VRAM_TILE_Y][T6_VRAM_TILE_X];
    unsigned int changedBm[64];
    unsigned int promotedBm[64];
    int tx, ty;

    T6VramMoveShadowDecisionInit(&before);
    expect_count(before.unresolvedTileX, -1);
    expect_count(before.unresolvedTileY, -1);
    before.result = VRAM_FRESH_MATERIALIZED;
    before.hazardTiles = 60;
    before.requiredTiles = 60;
    before.sourceHash = 0x1111u;
    for (ty = 0; ty < 15; ty++)
        for (tx = 16; tx < 20; tx++)
        {
            uint64_t tileSeq =
                (uint64_t)(1000 + ty * T6_VRAM_TILE_X + tx);

            before.requiredTile[ty][tx] = 1;
            before.requiredTileSeq[ty][tx] = tileSeq;
            before.requiredTileMapId[ty][tx] = 7;
        }

    memset(used, 0, sizeof(used));
    memset(mixed, 0, sizeof(mixed));
    memset(seq, 0, sizeof(seq));
    memset(source, 0, sizeof(source));
    memset(map, 0, sizeof(map));
    memset(changedBm, 0, sizeof(changedBm));
    memset(promotedBm, 0, sizeof(promotedBm));
    for (ty = 0; ty < 15; ty++)
        for (tx = 16; tx < 20; tx++)
        {
            int tileIndex = ty * T6_VRAM_TILE_X + tx;
            uint64_t tileSeq = (uint64_t)(1000 + tileIndex);

            expect_count(T6MoveRecordResolvedTileIfFull(
                             &used[0][0], &seq[0][0], &map[0][0],
                             &mixed[0][0], &source[0][0], tileIndex,
                             256, 0, tileSeq, 7,
                             (uint64_t)(2000 + tileIndex)), 1);
        }
    /* Only one tile changed; all 60 tiles are fully resolved and therefore
     * close their epoch, including the runtime-style zero-delta boundary. */
    changedBm[16 >> 5] |= 1u << (16 & 31);
    for (ty = 0; ty < 15; ty++)
        for (tx = 16; tx < 20; tx++)
        {
            int tileIndex = ty * T6_VRAM_TILE_X + tx;

            promotedBm[tileIndex >> 5] |= 1u << (tileIndex & 31);
            expect_u64(T6FullTileResolutionPromotionEpoch(
                           256, 1, (uint64_t)(1000 + tileIndex), 0),
                       (uint64_t)(1000 + tileIndex),
                       "fully resolved no-delta tile promotes epoch");
        }
    T6VramMoveOldPathEvidenceInit(&after);
    after.executed = 1;
    expect_count(T6MoveEvidenceFinalize(
                     &after, 1, 1, changedBm, 60, promotedBm, 1,
                     0x2222u, 7, 2000, used, mixed, seq, source, map), 1);
    expect_count((int)after.usedTiles, 60);
    expect_count(T6MoveShadowCompareDecisionEx(
                     &before, &after, &detail), T6_MOVE_COMPARE_MATCH);
    expect_count((int)detail.kind, (int)T6_MOVE_BAD_NONE);

    expect_u64(T6FullTileResolutionPromotionEpoch(255, 1, 13, 12), 12,
               "partial resolution keeps epoch");
    expect_u64(T6FullTileResolutionPromotionEpoch(256, 0, 13, 12), 12,
               "mixed resolution keeps epoch");
    expect_u64(T6FullTileResolutionPromotionEpoch(256, 1, 12, 12), 12,
               "non-newer resolution keeps epoch");

    /* Rebuild 59/60 evidence with the right-edge tile (19,2) genuinely
     * unresolved.  Finalize accepts the internally consistent old evidence,
     * but the equivalence comparator must reject the missing requirement. */
    memset(used, 0, sizeof(used));
    memset(mixed, 0, sizeof(mixed));
    memset(seq, 0, sizeof(seq));
    memset(source, 0, sizeof(source));
    memset(map, 0, sizeof(map));
    memset(promotedBm, 0, sizeof(promotedBm));
    for (ty = 0; ty < 15; ty++)
        for (tx = 16; tx < 20; tx++)
        {
            int tileIndex = ty * T6_VRAM_TILE_X + tx;

            if (tx == 19 && ty == 2)
                continue;
            expect_count(T6MoveRecordResolvedTileIfFull(
                             &used[0][0], &seq[0][0], &map[0][0],
                             &mixed[0][0], &source[0][0], tileIndex,
                             256, 0, (uint64_t)(1000 + tileIndex), 7,
                             (uint64_t)(2000 + tileIndex)), 1);
        }
    T6VramMoveOldPathEvidenceInit(&after);
    after.executed = 1;
    expect_count(T6MoveEvidenceFinalize(
                     &after, 1, 1, changedBm, 0, promotedBm, 1,
                     0x2222u, 7, 2000, used, mixed, seq, source, map), 1);
    expect_count((int)after.usedTiles, 59);
    expect_count(T6MoveShadowCompareDecisionEx(
                     &before, &after, &detail), T6_MOVE_COMPARE_MISMATCH);
    expect_count((int)detail.kind, (int)T6_MOVE_BAD_ABSENT);
    expect_count(detail.tx, 19);
    expect_count(detail.ty, 2);
    expect_count((int)detail.missingTiles, 1);

    /* Partial coverage, mixed source, and missing identity are never promoted
     * into resolved evidence. */
    expect_count(T6MoveRecordResolvedTileIfFull(
                     &used[0][0], &seq[0][0], &map[0][0],
                     &mixed[0][0], &source[0][0], 19, 255, 0,
                     1019, 7, 2019), 0);
    expect_count(T6MoveRecordResolvedTileIfFull(
                     &used[0][0], &seq[0][0], &map[0][0],
                     &mixed[0][0], &source[0][0], 19, 256, 1,
                     1019, 7, 2019), 0);
    expect_count(T6MoveRecordResolvedTileIfFull(
                     &used[0][0], &seq[0][0], &map[0][0],
                     &mixed[0][0], &source[0][0], 19, 256, 0,
                     1019, 7, 0), 0);
}

static void test_move_capture_preflight_and_cpu_provenance(void)
{
    T6CpuWriteProvenanceRing ring;
    T6CpuWriteProvenanceRing reuseRing;
    T6CpuWriteProvenance found;
    unsigned int i;

    expect_count(T6MoveCaptureBufferReady(0, 100, 100), 0);
    expect_count(T6MoveCaptureBufferReady(1, 99, 100), 0);
    expect_count(T6MoveCaptureBufferReady(1, 100, 100), 1);
    expect_count(T6MoveCaptureBufferReady(1, 101, 100), 1);
    expect_count(T6MoveCaptureBufferReady(1, 100, 0), 0);
    expect_count(T6MoveCaptureCandidateMatchesMap(7, 7), 1);
    expect_count(T6MoveCaptureCandidateMatchesMap(7, 8), 0);
    expect_count(T6MoveCaptureCandidateMatchesMap(0, 0), 0);

    T6CpuWriteProvenanceInit(&ring);
    memset(&found, 0, sizeof(found));
    expect_count(T6CpuWriteProvenanceFind(&ring, 1, &found), 0);
    T6CpuWriteProvenanceRecord(&ring, 0, T6_CPU_WRITE_A0,
                               1, 2, 3, 4, 0);
    expect_count((int)ring.count, 0);
    for (i = 1; i <= T6_CPU_WRITE_PROVENANCE_CAPACITY + 2u; i++)
        T6CpuWriteProvenanceRecord(
            &ring, (uint64_t)i,
            (i & 1u) ? T6_CPU_WRITE_A0 : T6_CPU_WRITE_MOVEIMAGE,
            (int)i, (int)i + 1, 16, 32,
            i == T6_CPU_WRITE_PROVENANCE_CAPACITY + 2u ?
                T6_CPU_WRITE_FLAG_UPLOAD_PENDING : 0);
    expect_count((int)ring.count, T6_CPU_WRITE_PROVENANCE_CAPACITY);
    expect_count(T6CpuWriteProvenanceFind(&ring, 1, &found), 0);
    expect_count(T6CpuWriteProvenanceFind(&ring, 2, &found), 0);
    expect_count(T6CpuWriteProvenanceFind(&ring, 3, &found), 1);
    expect_u64(found.seq, 3, "oldest retained CPU provenance sequence");
    expect_count(found.x, 3);
    expect_count(found.y, 4);
    expect_count(T6CpuWriteProvenanceFind(
                     &ring,
                     T6_CPU_WRITE_PROVENANCE_CAPACITY + 2u,
                     &found), 1);
    expect_count(found.kind, T6_CPU_WRITE_MOVEIMAGE);
    expect_count((int)found.flags,
                 T6_CPU_WRITE_FLAG_UPLOAD_PENDING);
    expect_count(T6CpuWriteProvenanceUpdate(
                     &ring, T6_CPU_WRITE_PROVENANCE_CAPACITY + 2u,
                     T6_CPU_WRITE_FLAG_CHECK_CALLED |
                     T6_CPU_WRITE_FLAG_UPLOAD_SUCCEEDED |
                     T6_CPU_WRITE_FLAG_BASELINE_ESTABLISHED,
                     77, 9), 1);
    expect_count(T6CpuWriteProvenanceFind(
                     &ring,
                     T6_CPU_WRITE_PROVENANCE_CAPACITY + 2u,
                     &found), 1);
    expect_count((int)found.flags,
                 T6_CPU_WRITE_FLAG_UPLOAD_PENDING |
                 T6_CPU_WRITE_FLAG_CHECK_CALLED |
                 T6_CPU_WRITE_FLAG_UPLOAD_SUCCEEDED |
                 T6_CPU_WRITE_FLAG_BASELINE_ESTABLISHED);
    expect_u64(found.baselineSeq, 77,
               "CPU provenance post-check baseline sequence");
    expect_count((int)found.baselineMapId, 9);
    expect_count(T6CpuWriteProvenanceUpdateA0Flow(
                     &ring, T6_CPU_WRITE_PROVENANCE_CAPACITY + 2u,
                     41, 52, 63, 60, 1, 1, 2, 0, 0,
                     0, 256, 320, 240, 2, 0, 2), 1);
    expect_count(T6CpuWriteProvenanceFind(
                     &ring,
                     T6_CPU_WRITE_PROVENANCE_CAPACITY + 2u,
                     &found), 1);
    expect_count((int)found.a0Generation, 41);
    expect_count((int)found.a0FinishSerial, 52);
    expect_count((int)found.a0DmaSerial, 63);
    expect_count((int)found.a0ArmDmaSerial, 60);
    expect_count(found.a0Armed, 1);
    expect_count(found.a0NeedUpload, 1);
    expect_count(found.a0WriteMode, 2);
    expect_count(found.a0ArmY, 256);
    expect_count(found.a0ArmW, 320);
    expect_count(found.a0ArmH, 240);
    expect_count(found.a0LastDmaMode, 2);
    expect_count(found.a0LastDmaOldWriteMode, 0);
    expect_count(found.a0LastDmaNewWriteMode, 2);
    expect_count(T6CpuWriteProvenanceUpdateA0Flow(
                     &ring, 9999, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                     1, 1, 1, 1, 1, 1, 1), 0);
    expect_count(T6CpuWriteProvenanceUpdate(
                     &ring, 9999,
                     T6_CPU_WRITE_FLAG_UPLOAD_DEFERRED, 88, 10), 0);
    expect_count(T6CpuWriteProvenanceFind(&ring, 9999, &found), 0);

    /* Duplicate sequence lookup returns the newest exact record. */
    T6CpuWriteProvenanceRecord(
        &ring, T6_CPU_WRITE_PROVENANCE_CAPACITY + 2u,
        T6_CPU_WRITE_BLKFILL, 9, 8, 7, 6,
        T6_CPU_WRITE_FLAG_FULL_REBUILD);
    expect_count(T6CpuWriteProvenanceFind(
                     &ring,
                     T6_CPU_WRITE_PROVENANCE_CAPACITY + 2u,
                     &found), 1);
    expect_count(found.kind, T6_CPU_WRITE_BLKFILL);
    expect_count(found.x, 9);
    expect_count((int)found.flags, T6_CPU_WRITE_FLAG_FULL_REBUILD);
    expect_count(T6CpuWriteProvenanceUpdate(
                     &ring, T6_CPU_WRITE_PROVENANCE_CAPACITY + 2u,
                     T6_CPU_WRITE_FLAG_UPLOAD_DEFERRED, 88, 10), 1);
    expect_count(T6CpuWriteProvenanceFind(
                     &ring,
                     T6_CPU_WRITE_PROVENANCE_CAPACITY + 2u,
                     &found), 1);
    expect_count(found.kind, T6_CPU_WRITE_BLKFILL);
    expect_count((int)found.flags,
                 T6_CPU_WRITE_FLAG_FULL_REBUILD |
                 T6_CPU_WRITE_FLAG_UPLOAD_DEFERRED);
    expect_u64(found.baselineSeq, 88,
               "newest duplicate provenance baseline sequence");
    expect_count((int)found.baselineMapId, 10);

    /* Reusing a ring slot must not let a MoveImage record inherit the
     * overwritten CPU upload's baseline identity. */
    T6CpuWriteProvenanceInit(&reuseRing);
    T6CpuWriteProvenanceRecord(
        &reuseRing, 1, T6_CPU_WRITE_A0, 1, 2, 3, 4,
        T6_CPU_WRITE_FLAG_UPLOAD_PENDING);
    expect_count(T6CpuWriteProvenanceUpdate(
                     &reuseRing, 1,
                     T6_CPU_WRITE_FLAG_BASELINE_ESTABLISHED, 77, 9), 1);
    expect_count(T6CpuWriteProvenanceUpdateA0Flow(
                     &reuseRing, 1, 7, 8, 9, 6, 1, 1, 2, 0, 0,
                     1, 2, 3, 4, 2, 0, 2), 1);
    for (i = 2; i <= T6_CPU_WRITE_PROVENANCE_CAPACITY; i++)
        T6CpuWriteProvenanceRecord(
            &reuseRing, (uint64_t)i, T6_CPU_WRITE_A0,
            (int)i, 0, 1, 1, 0);
    T6CpuWriteProvenanceRecord(
        &reuseRing, T6_CPU_WRITE_PROVENANCE_CAPACITY + 1u,
        T6_CPU_WRITE_MOVEIMAGE, 9, 8, 7, 6, 0);
    expect_count(T6CpuWriteProvenanceFind(
                     &reuseRing,
                     T6_CPU_WRITE_PROVENANCE_CAPACITY + 1u,
                     &found), 1);
    expect_count(found.kind, T6_CPU_WRITE_MOVEIMAGE);
    expect_u64(found.baselineSeq, 0,
               "reused provenance slot clears baseline sequence");
    expect_count((int)found.baselineMapId, 0);
    expect_count((int)found.a0Generation, 0);
    expect_count((int)found.a0FinishSerial, 0);
    expect_count(found.a0Armed, 0);
    expect_count(found.a0NeedUpload, 0);
    expect_count(found.a0ArmW, 0);
}

static void test_move_capture_preflight_source(void)
{
    static const char *readbackPaths[] = {
        "../GlesGpu/gpuVramReadback.inc",
        "WiiSXRX_2022/GlesGpu/gpuVramReadback.inc",
        "GlesGpu/gpuVramReadback.inc"
    };
    static const char *pluginPaths[] = {
        "../GlesGpu/gpuPlugin.c",
        "WiiSXRX_2022/GlesGpu/gpuPlugin.c",
        "GlesGpu/gpuPlugin.c"
    };
    static const char *primPaths[] = {
        "../GlesGpu/gpuPrim.c",
        "WiiSXRX_2022/GlesGpu/gpuPrim.c",
        "GlesGpu/gpuPrim.c"
    };
    int i;
    int readbackOpened = 0, readbackOrdered = 0;
    int pluginOpened = 0, pluginWired = 0;
    int primOpened = 0, primWired = 0;

    for (i = 0; i < 3; i++)
    {
        char *buf = read_whole_file(readbackPaths[i]);
        char *start, *plan, *wouldCapture, *bufferReady;
        char *postValidate, *materialized;

        if (buf == NULL)
            continue;
        readbackOpened = 1;
        start = strstr(buf, "static void T6VramMoveShadowDecisionForRect(");
        plan = start != NULL ?
               strstr(start, "T6VramMoveCapturePlanForSession(") : NULL;
        wouldCapture = plan != NULL ?
                       strstr(plan, "out->wouldCapture = 1") : NULL;
        bufferReady = wouldCapture != NULL ?
                      strstr(wouldCapture,
                             "T6VramMoveCaptureSlotBufferReady(") : NULL;
        postValidate = bufferReady != NULL ?
                       strstr(bufferReady,
                              "T6VramMoveCapturePostValidateForSession(") :
                       NULL;
        materialized = postValidate != NULL ?
                       strstr(postValidate,
                              "out->result = VRAM_FRESH_MATERIALIZED") :
                       NULL;
        if (start != NULL && plan != NULL && wouldCapture != NULL &&
            bufferReady != NULL && postValidate != NULL &&
            materialized != NULL &&
            strstr(buf, "T6MoveCaptureBufferReady(") != NULL &&
            strstr(buf, "T6MoveCaptureCandidateMatchesMap(") != NULL &&
            start < plan && plan < wouldCapture &&
            wouldCapture < bufferReady && bufferReady < postValidate &&
            postValidate < materialized)
            readbackOrdered = 1;
        free(buf);
        if (readbackOrdered)
            break;
    }

    for (i = 0; i < 3; i++)
    {
        char *buf = read_whole_file(pluginPaths[i]);

        if (buf == NULL)
            continue;
        pluginOpened = 1;
        if (strstr(buf, "MarkCpuVramWriteKind(") != NULL &&
            strstr(buf, "T6_CPU_WRITE_A0") != NULL &&
            strstr(buf, "T6_CPU_WRITE_FLAG_UPLOAD_PENDING") != NULL &&
            strstr(buf, "T6CpuWriteProvenanceUpdate(") != NULL &&
            strstr(buf, "T6CpuWriteProvenanceUpdateA0Flow(") != NULL &&
            strstr(buf, "g_t6A0LastDmaOldWriteMode") != NULL &&
            strstr(buf, "g_t6A0TransferArmed = 0") != NULL &&
            strstr(buf, "g_rebuildBaseline.capturedContentSeq") != NULL)
            pluginWired = 1;
        free(buf);
        if (pluginWired)
            break;
    }

    for (i = 0; i < 3; i++)
    {
        char *buf = read_whole_file(primPaths[i]);

        if (buf == NULL)
            continue;
        primOpened = 1;
        if (strstr(buf, "MarkCpuVramWriteWithSeqKind(") != NULL &&
            strstr(buf, "T6_CPU_WRITE_BLKFILL") != NULL &&
            strstr(buf, "T6_CPU_WRITE_MOVEIMAGE") != NULL &&
            strstr(buf, "T6CpuWriteProvenanceFind(") != NULL &&
            strstr(buf, "T6_CPU_WRITE_FLAG_UPLOAD_SUCCEEDED") != NULL &&
            strstr(buf, "T6_CPU_WRITE_FLAG_UPLOAD_DEFERRED") != NULL &&
            strstr(buf, "g_t6A0TransferGeneration++") != NULL &&
            strstr(buf, "g_t6A0TransferArmed = 1") != NULL &&
            strstr(buf, "TRB MOVE A0 seq=%llu") != NULL &&
            strstr(buf, "TRB MOVE TAKE") != NULL &&
            strstr(buf, "hash=%08X->%08X") != NULL &&
            strstr(buf, "cpuBase=%llu/%u") != NULL)
            primWired = 1;
        free(buf);
        if (primWired)
            break;
    }

    expect_bool(readbackOpened, 1, "readback source opened for preflight");
    expect_bool(readbackOrdered, 1,
                "capture plan < buffer gate < post-validate < materialized");
    expect_bool(pluginOpened, 1, "gpuPlugin.c source opened for provenance");
    expect_bool(pluginWired, 1,
                "A0 finish/GP1 DMA lifecycle provenance wired");
    expect_bool(primOpened, 1, "gpuPrim.c source opened for provenance");
    expect_bool(primWired, 1,
                "A0 arm plus BlkFill/MoveImage/TILE diagnostics wired");
}

static void test_move_post_generic_regression(void)
{
    /* P2-F3 真值表：generic 成功 + post 非 clean NO_ACTION 计 regression
     * （每个 MoveImage 最多一次）；generic unresolved 不计（预期
     * fail-closed 状态，由 takeUnresolved 统计）。 */
    /* generic success + clean post：不计 */
    expect_count(T6MovePostGenericRegression(
                     (int)VRAM_FRESH_MATERIALIZED,
                     (int)VRAM_FRESH_NO_ACTION,
                     T6_REASON_NONE, 0), 0);
    expect_count(T6MovePostGenericRegression(
                     (int)VRAM_FRESH_NO_ACTION,
                     (int)VRAM_FRESH_NO_ACTION,
                     T6_REASON_NONE, 0), 0);
    /* generic success + post 非 NO_ACTION：计 */
    expect_count(T6MovePostGenericRegression(
                     (int)VRAM_FRESH_MATERIALIZED,
                     (int)VRAM_FRESH_MATERIALIZED,
                     T6_REASON_NONE, 0), 1);
    expect_count(T6MovePostGenericRegression(
                     (int)VRAM_FRESH_NO_ACTION,
                     (int)VRAM_FRESH_UNRESOLVED,
                     T6_REASON_HAZARD, 1), 1);
    /* generic success + postReason 非 NONE（即使 result=NO_ACTION）：计 */
    expect_count(T6MovePostGenericRegression(
                     (int)VRAM_FRESH_MATERIALIZED,
                     (int)VRAM_FRESH_NO_ACTION,
                     T6_REASON_PARTIAL, 0), 1);
    /* generic success + postHazard 非零（即使 result=NO_ACTION）：计 */
    expect_count(T6MovePostGenericRegression(
                     (int)VRAM_FRESH_MATERIALIZED,
                     (int)VRAM_FRESH_NO_ACTION,
                     T6_REASON_NONE, 3), 1);
    /* generic unresolved：不计 regression */
    expect_count(T6MovePostGenericRegression(
                     (int)VRAM_FRESH_UNRESOLVED,
                     (int)VRAM_FRESH_UNRESOLVED,
                     T6_REASON_RGB24, 5), 0);
    expect_count(T6MovePostGenericRegression(
                     (int)VRAM_FRESH_UNRESOLVED,
                     (int)VRAM_FRESH_NO_ACTION,
                     T6_REASON_NONE, 0), 0);
    /* 非法 generic：不计 success-post regression。 */
    expect_count(T6MovePostGenericRegression(
                     99, (int)VRAM_FRESH_UNRESOLVED,
                     T6_REASON_HAZARD, 2), 0);
}

static void test_move_timing_delta(void)
{
    /* P2-F4: totals 差分（无副作用），单次 delta 饱和到 UINT_MAX。
     * early-return 路径（totals 未变）delta 自然为 0。 */
    expect_count(T6MoveTimingDeltaUs(1000, 1000), 0);
    expect_count(T6MoveTimingDeltaUs(1000, 1123), 123);
    expect_count(T6MoveTimingDeltaUs(0, 0), 0);
    expect_count(T6MoveTimingDeltaUs(0, 12345), 12345);
    /* 超 uint32 饱和 */
    expect_count(T6MoveTimingDeltaUs(0, 0x1FFFFFFFFull), 0xFFFFFFFFu);
    expect_count(T6MoveTimingDeltaUs(0xFFFFFFFFull, 0x100000000ull), 1);
}

static void test_move_i_positive(void)
{
    /* P1-F1 测试 I 正例（真实 materialize harness，非手写期望值）：
     * EFB-newer source tile（snapshot/physical seq 新于 CPU epoch）→
     * harness_run() 必须 MATERIALIZED、writes=256、matEpoch 提交（真实
     * session 状态机）；然后对同一 h->vram 调共享 copy core；destination
     * 必须等于 T6MergePixelColor(背景, harness_color()) | mask，source 外
     * 背景与 destination 外背景不变、source 保留。删除 materialize/commit
     * 或把 EFB-newer 误判为 NO_ACTION 都会使本测试失败。 */
    T6Harness *h = &s_harness;
    VramReadDependency dep;
    const unsigned short mask16 = 0x8000;
    const unsigned long mask32 = 0x80008000ul;
    const int sx0 = 0, sy0 = 0, dx0 = 300, dy0 = 16;
    int px, py;

    harness_reset(h);
    memset(&dep, 0, sizeof(dep));
    append_tile_dep(&dep, 0, 0);

    h->cpuEpoch[0][0] = 10;
    h->physPresent[0][0] = 1;
    h->physSeq[0][0] = 11;
    h->snapshotPresent[0][0] = 1;
    h->snapshotFull[0][0] = 1;
    h->snapshotSeq[0][0] = 11;
    h->snapshotSlot[0][0] = 0;

    expect_count((int)harness_run(h, &dep),
                 (int)VRAM_FRESH_MATERIALIZED);
    expect_count(h->captures, 0);
    expect_count(h->writes, 256);
    expect_u64(h->matEpoch[0][0], 11, "EFB-newer epoch committed");

    T6MoveCopyRect(h->vram, 512, sx0, sy0, dx0, dy0, 16, 16,
                   mask32, mask16);

    /* dest 像素 (dx0+i, dy0+j) 来自 source 像素 (i, j)，期望值必须用
     * source 坐标的 harness_color。 */
    for (py = dy0; py < dy0 + 16; py++)
        for (px = dx0; px < dx0 + 16; px++)
        {
            uint16_t expect = (uint16_t)(T6MergePixelColor(
                0x8000u, harness_color(0, 0,
                                       px - dx0, py - dy0)) | mask16);

            expect_bool((int)h->vram[py * 1024 + px], (int)expect,
                        "dest pixel == merged GX | mask");
        }
    /* source 外背景与 dest 外背景（坐标都在未被 materialize 的 tile 上，
     * 保持 harness_reset 的 0x8000）：source (0,0) 16x16 = tile(0,0)，其
     * 相邻 tile (1,0)/(0,1) 未配置 EFB → 0x8000。 */
    expect_bool((int)h->vram[0 * 1024 + 16], 0x8000,
                "source 右背景不变");
    expect_bool((int)h->vram[16 * 1024 + 0], 0x8000,
                "source 下背景不变");
    expect_bool((int)h->vram[15 * 1024 + 300], 0x8000,
                "dest 上背景不变");
    expect_bool((int)h->vram[32 * 1024 + 300], 0x8000,
                "dest 下背景不变");
    expect_bool((int)h->vram[16 * 1024 + 299], 0x8000,
                "dest 左背景不变");
    expect_bool((int)h->vram[16 * 1024 + 316], 0x8000,
                "dest 右背景不变");
    expect_bool((int)h->vram[8 * 1024 + 8],
                (int)T6MergePixelColor(0x8000u,
                                       harness_color(0, 0, 8, 8)),
                "source 内容保留");
}

static void test_move_i_negative(void)
{
    /* P1-F1 测试 I 负例（真实 harness）：CPU epoch 大于 snapshot/physical
     * seq（CPU-newer）→ harness_run() 必须 NO_ACTION、writes=0、matEpoch
     * 未提升；source VRAM 预放 A0 内容；copy 后 destination 完全来自 A0
     * （旧 EFB 不重新覆盖）。若实现把 CPU-newer 误判成 MATERIALIZED
     * （writes>0 / epoch 提升 / dest 被 EFB 覆盖）本测试失败。 */
    T6Harness *h = &s_harness;
    VramReadDependency dep;
    const unsigned short a0Value = 0x00FF;
    const unsigned short mask16 = 0x8000;
    const unsigned long mask32 = 0x80008000ul;
    const int sx0 = 0, sy0 = 0, dx0 = 300, dy0 = 16;
    int px, py;

    harness_reset(h);
    memset(&dep, 0, sizeof(dep));
    append_tile_dep(&dep, 0, 0);

    h->cpuEpoch[0][0] = 100;
    h->physPresent[0][0] = 1;
    h->physSeq[0][0] = 50;
    h->snapshotPresent[0][0] = 1;
    h->snapshotFull[0][0] = 1;
    h->snapshotSeq[0][0] = 50;
    h->snapshotSlot[0][0] = 0;
    for (py = 0; py < 16; py++)
        for (px = 0; px < 16; px++)
            h->vram[py * 1024 + px] = a0Value;

    expect_count((int)harness_run(h, &dep),
                 (int)VRAM_FRESH_NO_ACTION);
    expect_bool(h->writes, 0, "CPU-newer 不 materialize");
    expect_u64(h->matEpoch[0][0], 0, "CPU-newer matEpoch 未提升");
    T6MoveCopyRect(h->vram, 512, sx0, sy0, dx0, dy0, 16, 16,
                   mask32, mask16);
    for (py = dy0; py < dy0 + 16; py++)
        for (px = dx0; px < dx0 + 16; px++)
            expect_bool((int)h->vram[py * 1024 + px],
                        (int)(a0Value | mask16),
                        "dest 完全来自 A0，旧 EFB 不覆盖");
}

static void test_move_no_delta_second_read(void)
{
    /* P1-F1 no-delta（真实 harness，非伪状态机）：source tile 预填为
     * materialize 后完全相同的内容（harness_prefill_tile）→ 第一次
     * harness_run() 必须 MATERIALIZED、writes=0、changedCount=0、matEpoch
     * 提交（no-delta 也提交 epoch）；第二次/第三次同 dependency 必须
     * NO_ACTION。删除 epoch commit 或改成 changed=0 就不提交都会失败。 */
    T6Harness *h = &s_harness;
    VramReadDependency dep;

    harness_reset(h);
    memset(&dep, 0, sizeof(dep));
    append_tile_dep(&dep, 0, 0);

    h->cpuEpoch[0][0] = 10;
    h->physPresent[0][0] = 1;
    h->physSeq[0][0] = 11;
    h->snapshotPresent[0][0] = 1;
    h->snapshotFull[0][0] = 1;
    h->snapshotSeq[0][0] = 11;
    h->snapshotSlot[0][0] = 0;
    harness_prefill_tile(h, 0, 0);

    expect_count((int)harness_run(h, &dep),
                 (int)VRAM_FRESH_MATERIALIZED);
    expect_bool(h->writes, 0, "no-delta writes=0");
    expect_bool((int)h->changed[0][0], 0, "no-delta changed=0");
    expect_u64(h->matEpoch[0][0], 11, "no-delta epoch 提交");

    expect_count((int)harness_run(h, &dep),
                 (int)VRAM_FRESH_NO_ACTION);
    expect_count((int)harness_run(h, &dep),
                 (int)VRAM_FRESH_NO_ACTION);

    /* precheck 拒绝路径：totals 不变 → delta=0（P2-F4 无副作用） */
    expect_count(T6MoveSourceFreshnessPrecheck(
                     10, 10, 8, 8, 512, 0, 0, 0), 0);
    expect_count(T6MoveSourceFreshnessPrecheck(
                     10, 10, 8, 8, 1024, 1, 0, 0), 0);
}

/* P2-F6: pre-R3 reference copy——独立复制自移动前 gpuPrim.c 的
 * MoveImageWrapped()（WiiSXRX_2022 legacy formula），不调用被测
 * T6MoveCopyRect。wrap 分支不应用 mask；unaligned 16-bit OR setMask16；
 * aligned 32-bit OR setMask32。vramHeight 参数化以支持 512/1024-line
 * （512-line 时 wrap mask = 511 与移动前 &511 一致）。 */
static void t6_ref_move_copy(unsigned short *vram, int vramHeight,
                             int imageX0, int imageY0,
                             int imageX1, int imageY1,
                             int imageSX, int imageSY,
                             unsigned long setMask32,
                             unsigned short setMask16)
{
    int i, j;
    int wrapMask = vramHeight - 1;

    if ((imageY0 + imageSY) > vramHeight ||
        (imageX0 + imageSX) > 1024 ||
        (imageY1 + imageSY) > vramHeight ||
        (imageX1 + imageSX) > 1024)
    {
        for (j = 0; j < imageSY; j++)
            for (i = 0; i < imageSX; i++)
                vram[(1024 * ((imageY1 + j) & wrapMask)) +
                     ((imageX1 + i) & 0x3ff)] =
                    vram[(1024 * ((imageY0 + j) & wrapMask)) +
                         ((imageX0 + i) & 0x3ff)];
        return;
    }
    if ((imageSX | imageX0 | imageX1) & 1)
    {
        unsigned short *SRCPtr = vram + 1024 * imageY0 + imageX0;
        unsigned short *DSTPtr = vram + 1024 * imageY1 + imageX1;
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

static void t6_fill_vram(unsigned short *vram, int n, unsigned int seed)
{
    unsigned int s = seed;
    int i;

    for (i = 0; i < n; i++)
    {
        s = s * 1103515245u + 12345u;
        vram[i] = (unsigned short)(s >> 16);
    }
}

static unsigned int t6_vram_hash(const unsigned short *vram, int n)
{
    unsigned int hash = 2166136261u;
    int i;

    for (i = 0; i < n; i++)
        hash = (hash ^ (unsigned int)vram[i]) * 16777619u;
    return hash;
}

/* P2-F6: 共享 copy core 全分支等价 Gate——aligned/unaligned/wrap/overlap/
 * 512/1024 全部与 pre-R3 reference 在相同初始 VRAM 上逐字节等价；并断言
 * wrap 不应用 mask、非 wrap 应用 mask。 */
static void test_move_copy_core_equivalence(void)
{
    static unsigned short vA[1024 * 1024];
    static unsigned short vB[1024 * 1024];
    static unsigned short snapshot[1024 * 1024];
    const struct
    {
        const char *name;
        int sx, sy, dx, dy, w, h;
        int height;
        unsigned long mask32;
        unsigned short mask16;
    } cases[] = {
        /* aligned 32-bit（mask 应用） */
        { "aligned 64x32", 16, 16, 300, 16, 64, 32, 512,
          0x80008000ul, 0x8000 },
        /* aligned 多行大矩形 */
        { "aligned 256x128", 0, 0, 700, 300, 256, 128, 512,
          0x80008000ul, 0x8000 },
        /* unaligned：source odd */
        { "source odd", 17, 16, 300, 16, 64, 32, 512,
          0x80008000ul, 0x8000 },
        /* unaligned：destination odd */
        { "dest odd", 16, 16, 301, 16, 64, 32, 512,
          0x80008000ul, 0x8000 },
        /* unaligned：odd width */
        { "odd width 63", 16, 16, 300, 16, 63, 32, 512,
          0x80008000ul, 0x8000 },
        /* wrap：X（dest 选不重叠区域以断言不应用 mask） */
        { "X wrap", 1020, 16, 500, 16, 8, 8, 512,
          0x80008000ul, 0x8000 },
        /* wrap：Y */
        { "Y wrap", 16, 508, 16, 500, 8, 8, 512,
          0x80008000ul, 0x8000 },
        /* wrap：X+Y */
        { "X+Y wrap", 1020, 508, 500, 500, 8, 8, 512,
          0x80008000ul, 0x8000 },
        /* overlap：dest 与 source 部分重叠（只做 ref/core 等价，不设
         * 逐像素期望——行内读先于写的覆盖语义由等价性锁定） */
        { "overlap partial", 16, 16, 48, 16, 64, 32, 512,
          0x80008000ul, 0x8000 },
        /* overlap：dest 与 source 完全同区域 */
        { "overlap same", 16, 16, 16, 16, 64, 32, 512,
          0x80008000ul, 0x8000 },
        /* 1024-line aligned */
        { "1024 aligned", 16, 16, 300, 500, 64, 32, 1024,
          0x80008000ul, 0x8000 },
        /* 1024-line wrap（mask=1023，dest 不重叠） */
        { "1024 X+Y wrap", 1020, 1020, 500, 500, 8, 8, 1024,
          0x80008000ul, 0x8000 },
    };
    unsigned int i;

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        const int n = 1024 * cases[i].height;
        const int wrap = (cases[i].sy + cases[i].h) > cases[i].height ||
                         (cases[i].sx + cases[i].w) > 1024 ||
                         (cases[i].dy + cases[i].h) > cases[i].height ||
                         (cases[i].dx + cases[i].w) > 1024;
        int sp, dp;

        t6_fill_vram(vA, n, 0x9E3779B9u ^ (i * 2654435761u));
        memcpy(vB, vA, (size_t)n * sizeof(unsigned short));

        /* 执行前快照 source 区域（dest 同形） */
        memcpy(snapshot, vA, (size_t)n * sizeof(unsigned short));

        t6_ref_move_copy(vA, cases[i].height,
                         cases[i].sx, cases[i].sy,
                         cases[i].dx, cases[i].dy,
                         cases[i].w, cases[i].h,
                         cases[i].mask32, cases[i].mask16);
        T6MoveCopyRect(vB, cases[i].height,
                       cases[i].sx, cases[i].sy,
                       cases[i].dx, cases[i].dy,
                       cases[i].w, cases[i].h,
                       cases[i].mask32, cases[i].mask16);

        expect_bool((int)(t6_vram_hash(vA, n) ==
                          t6_vram_hash(vB, n)), 1,
                    cases[i].name);
        expect_bool(memcmp(vA, vB, (size_t)n * sizeof(unsigned short)) == 0,
                    1, cases[i].name);

        /* mask 语义：非 wrap 且 dest/source 不重叠的用例断言
         * dest == source|mask16；wrap 且不重叠的用例断言 dest == source
         * 原值（不应用 mask）。overlap 用例的逐像素覆盖行为由 ref/core
         * 等价性锁定，不另设期望公式。 */
        if (!(cases[i].dy < cases[i].sy + cases[i].h &&
              cases[i].dy + cases[i].h > cases[i].sy &&
              cases[i].dx < cases[i].sx + cases[i].w &&
              cases[i].dx + cases[i].w > cases[i].sx))
        {
            for (sp = 0; sp < cases[i].h; sp++)
                for (dp = 0; dp < cases[i].w; dp++)
                {
                    int syy = (cases[i].sy + sp) & (cases[i].height - 1);
                    int sxx = (cases[i].sx + dp) & 0x3ff;
                    int dyy = (cases[i].dy + sp) & (cases[i].height - 1);
                    int dxx = (cases[i].dx + dp) & 0x3ff;
                    unsigned short srcVal =
                        snapshot[syy * 1024 + sxx];
                    unsigned short got =
                        vB[dyy * 1024 + dxx];
                    unsigned short want =
                        wrap ? srcVal :
                               (unsigned short)(srcVal | cases[i].mask16);

                    if (got != want)
                    {
                        expect_bool(0, 1, cases[i].name);
                        break;
                    }
                }
        }
    }
}

static void test_move_source_freshness_precheck(void)
{
    /* 134.5.5 fail-closed 矩阵：1024-line、非法 rect、readback disabled、
     * reentrant 全部拒绝且零写入；合法 512-line 输入通过。wrap 是合法输入：
     * X/Y/X+Y wrap dependency 由 BuildMoveSourceDependency 分段处理，预检只
     * 验证起点与正值尺寸（x=1020, w=8 是合法的 X wrap）。 */
    expect_count(T6MoveSourceFreshnessPrecheck(
                     10, 10, 8, 8, 512, 1, 0, 0), 1);
    expect_count(T6MoveSourceFreshnessPrecheck(
                     1020, 10, 8, 8, 512, 1, 0, 0), 1);
    expect_count(T6MoveSourceFreshnessPrecheck(
                     10, 508, 8, 8, 512, 1, 0, 0), 1);
    expect_count(T6MoveSourceFreshnessPrecheck(
                     10, 10, 8, 8, 512, 0, 0, 0), 0);
    expect_count(T6MoveSourceFreshnessPrecheck(
                     10, 10, 0, 8, 512, 1, 0, 0), 0);
    expect_count(T6MoveSourceFreshnessPrecheck(
                     10, 10, 8, 0, 512, 1, 0, 0), 0);
    expect_count(T6MoveSourceFreshnessPrecheck(
                     10, 10, -8, 8, 512, 1, 0, 0), 0);
    expect_count(T6MoveSourceFreshnessPrecheck(
                     -1, 10, 8, 8, 512, 1, 0, 0), 0);
    expect_count(T6MoveSourceFreshnessPrecheck(
                     1024, 10, 8, 8, 512, 1, 0, 0), 0);
    expect_count(T6MoveSourceFreshnessPrecheck(
                     10, -1, 8, 8, 512, 1, 0, 0), 0);
    expect_count(T6MoveSourceFreshnessPrecheck(
                     10, 512, 8, 8, 512, 1, 0, 0), 0);
    expect_count(T6MoveSourceFreshnessPrecheck(
                     10, 10, 8, 8, 256, 1, 0, 0), 0);
    expect_count(T6MoveSourceFreshnessPrecheck(
                     10, 10, 8, 8, 1024, 1, 0, 0), 0);
    expect_count(T6MoveSourceFreshnessPrecheck(
                     10, 10, 8, 8, 512, 1, 1, 0), 0);
    expect_count(T6MoveSourceFreshnessPrecheck(
                     10, 10, 8, 8, 512, 1, 0, 1), 0);
}

static void test_move_diag_workspace_layout(void)
{
    static const char *paths[] = {
        "../GlesGpu/gpuVramReadback.inc",
        "WiiSXRX_2022/GlesGpu/gpuVramReadback.inc",
        "GlesGpu/gpuVramReadback.inc"
    };
    size_t ws = sizeof(T6MoveTakeDiagWorkspace);
    size_t two = sizeof(VramMoveShadowDecision) * 2;
    int i, opened = 0, dynamicOk = 0;

    expect_bool(ws >= two + sizeof(int) ? 1 : 0, 1,
                 "workspace 包含 generic before/post + busy");
    expect_bool(ws > 50000 ? 1 : 0, 1,
                 "workspace 承载约 53 KiB generic evidence");
    expect_bool(ws < 200000 ? 1 : 0, 1,
                 "workspace 有界（防未来无限膨胀）");

    /* Hang A/B: large evidence must no longer occupy MEM1 .bss. */
    for (i = 0; i < 3; i++)
    {
        char *buf = read_whole_file(paths[i]);

        if (buf == NULL)
            continue;
        opened = 1;
        if (strstr(buf, "static T6MoveTakeDiagWorkspace "
                        "g_t6MoveTakeDiag;") == NULL &&
            strstr(buf, "_mem2_memalign(") != NULL &&
            strstr(buf, "T6MoveTakeDiagEnsureWorkspace") != NULL &&
            strstr(buf, "T6_MOVE_DIAG_GUARD_BEFORE") != NULL &&
            strstr(buf, "T6_MOVE_DIAG_GUARD_AFTER") != NULL)
            dynamicOk = 1;
        free(buf);
        if (dynamicOk)
            break;
    }
    expect_bool(opened, 1, "gpuVramReadback.inc opened for DIAG workspace");
    expect_bool(dynamicOk, 1,
                "large DIAG workspace is guarded and allocated from MEM2");
}

static void test_move_takeover_source_order(void)
{
    static const char *paths[] = {
        "../GlesGpu/gpuPrim.c",
        "WiiSXRX_2022/GlesGpu/gpuPrim.c",
        "GlesGpu/gpuPrim.c"
    };
    static const char *readbackPaths[] = {
        "../GlesGpu/gpuVramReadback.inc",
        "WiiSXRX_2022/GlesGpu/gpuVramReadback.inc",
        "GlesGpu/gpuVramReadback.inc"
    };
    int i, opened = 0, ordered = 0;
    int wrapperOpened = 0, wrapperOk = 0;

    for (i = 0; i < 3; i++)
    {
        char *buf = read_whole_file(paths[i]);
        char *sel, *funcEnd, *before, *generic, *post;
        char *workspace, *logGate, *copy, *mark, *inval, *deltaA;
        char *capTotalA;

        if (buf == NULL)
            continue;
        opened = 1;
        sel = strstr(buf, "static void primMoveImage (");
        if (sel == NULL)
        {
            free(buf);
            continue;
        }
        funcEnd = strstr(sel + 1, "\nstatic void ");
        if (funcEnd == NULL)
            funcEnd = buf + strlen(buf);
        before = strstr(sel, "T6VramMoveShadowDecisionForRect(");
        workspace = strstr(sel, "T6MoveTakeDiagEnsureWorkspace()");
        generic = before != NULL ?
                  strstr(before, "EnsureVramMoveSourceFresh(") : NULL;
        post = generic != NULL ?
               strstr(generic, "T6VramMoveShadowDecisionForRect(") : NULL;
        logGate = post != NULL ?
                  strstr(post, "if (isLogFileEnabled() &&") : NULL;
        copy = strstr(sel, "T6MoveCopyRect(psxVuw,");
        mark = strstr(sel, "MarkCpuVramWriteKind(");
        inval = strstr(sel, "InvalidateTextureArea(");
        /* totals 差分快照在 generic 之后；total 读取在 generic 之前。 */
        deltaA = strstr(generic != NULL ? generic : sel,
                        "T6MoveTimingDeltaUs(");
        capTotalA = strstr(sel, "capBefore = "
                           "g_vramReadBarrierCaptureUsTotal");
        if (workspace != NULL && before != NULL && generic != NULL && post != NULL &&
            logGate != NULL &&
            copy != NULL && mark != NULL && inval != NULL &&
            deltaA != NULL && capTotalA != NULL &&
            capTotalA < generic &&
            workspace < before && before < generic &&
            generic < deltaA && deltaA < post &&
            post < logGate && logGate < copy &&
            copy < mark && mark < inval && inval < funcEnd &&
            /* T6-E-3：generic 调用严格早于 CPU copy；DC2 predicate、路由
             * helper 与 old materialize helper 必须完全退出生产函数。 */
            count_text_in_range(sel, funcEnd,
                                "EnsureVramMoveSourceFresh(") == 1 &&
            count_text_in_range(sel, funcEnd,
                                "MaterializeEfbForVramMove(") == 0 &&
            count_text_in_range(sel, funcEnd,
                                "T6MoveTakeoverNeedsFallback(") == 0 &&
            count_text_in_range(sel, funcEnd, "dc2Predicate") == 0 &&
            count_text_in_range(sel, funcEnd,
                                "AUTO_FIX_DINO_CRISIS2") == 0 &&
            count_text_in_range(sel, funcEnd,
                                "T6VramMoveShadowDecisionForRect(") == 2 &&
            count_text_in_range(sel, funcEnd,
                                "T6MoveShadowCompareDecisionEx(") == 0 &&
            count_text_in_range(sel, funcEnd,
                                "TRB MOVE TAKE") == 1 &&
            count_text_in_range(sel, funcEnd,
                                "TRB MOVE BUSY") == 1 &&
            /* 两份大 evidence 不得是 command 栈局部变量；MEM2 workspace
             * 必须先 ensure，再使用 busy guard。 */
            count_text_in_range(sel, funcEnd,
                                "VramMoveShadowDecision beforeShadow;") == 0 &&
            count_text_in_range(sel, funcEnd,
                                "VramMoveShadowDecision postShadow;") == 0 &&
            count_text_in_range(sel, funcEnd,
                                "VramMoveOldPathEvidence afterEvidence;") == 0 &&
            count_text_in_range(sel, funcEnd,
                                "g_t6MoveTakeDiag.after") == 0 &&
            count_text_in_range(sel, funcEnd,
                                "g_t6MoveTakeDiag.") >= 4 &&
            count_text_in_range(sel, funcEnd,
                                "g_t6MoveTakeDiag.busy") >= 2 &&
            count_text_in_range(sel, funcEnd,
                                "T6MoveTakeDiagEnsureWorkspace()") == 1 &&
            count_text_in_range(sel, funcEnd,
                                "TRB MOVE BUSY") == 1 &&
            /* P2-F4: generic 调用前不得再清共享 per-call 字段。 */
            count_text_in_range(sel, funcEnd,
                                "g_vramReadBarrierCaptureUs = 0") == 0 &&
            count_text_in_range(sel, funcEnd,
                                "g_vramReadBarrierWriteUs = 0") == 0 &&
            count_text_in_range(sel, funcEnd,
                                "g_vramReadBarrierInvalUs = 0") == 0 &&
            /* P2-F4: 日志字段是本次 delta 命名。 */
            count_text_in_range(sel, funcEnd,
                                "capDeltaUs=%u") == 1 &&
            count_text_in_range(sel, funcEnd,
                                "finalReason=%d finalHazard=%u") == 0 &&
            count_text_in_range(sel, funcEnd, "fallback=%d") == 0 &&
            /* BUSY、TAKE、workspace failure 日志仍受门控。 */
            count_text_in_range(sel, funcEnd,
                                "if (isLogFileEnabled() && "
                                "g_vramMoveTakeLogs < 32)") == 3 &&
            /* P3-F8: copy loops 移入共享 core 后 primMoveImage 不得再
             * 声明未使用的 i, j。 */
            count_text_in_range(sel, funcEnd,
                                "imageSY, i, j") == 0)
            ordered = 1;
        free(buf);
        if (ordered)
            break;
    }
    expect_bool(opened, 1, "gpuPrim.c source opened");
    expect_bool(ordered, 1,
                "before shadow < generic < timing delta < post < log < "
                "copy(T6MoveCopyRect) < MarkCpuVramWrite < invalidate; "
                "T6-E-3 old fallback absent");

    for (i = 0; i < 3; i++)
    {
        char *buf = read_whole_file(readbackPaths[i]);
        char *wrap, *end, *dep, *fresh;

        if (buf == NULL)
            continue;
        wrapperOpened = 1;
        wrap = strstr(buf, "static VramFreshResult "
                           "EnsureVramMoveSourceFresh(");
        end = wrap != NULL ? strstr(wrap, "\n}\n") : NULL;
        dep = wrap != NULL ?
              strstr(wrap, "BuildMoveSourceDependency(") : NULL;
        fresh = wrap != NULL ?
                strstr(wrap, "EnsureVramReadFreshEx(") : NULL;
        if (wrap != NULL && end != NULL && dep != NULL && fresh != NULL &&
            dep < fresh && fresh < end &&
            count_text_in_range(wrap, end,
                                "T6MoveSourceFreshnessPrecheck(") == 1 &&
            count_text_in_range(wrap, end, "MoveImageWrapped") == 0 &&
            count_text_in_range(wrap, end, "MarkCpuVramWrite") == 0 &&
            count_text_in_range(wrap, end,
                                "MaterializeEfbForVramMove(") == 0 &&
            strstr(buf, "static void MaterializeEfbForVramMove(") == NULL)
            wrapperOk = 1;
        free(buf);
        if (wrapperOk)
            break;
    }
    expect_bool(wrapperOpened, 1,
                "gpuVramReadback.inc opened for move wrapper");
    expect_bool(wrapperOk, 1,
                "wrapper: precheck < dep < EnsureVramReadFreshEx; no copy/"
                "ownership; old DC2 helper removed");
}

static void test_cpu_newer_probe_source_order(void)
{
    static const char *pluginPaths[] = {
        "../GlesGpu/gpuPlugin.c",
        "WiiSXRX_2022/GlesGpu/gpuPlugin.c",
        "GlesGpu/gpuPlugin.c"
    };
    static const char *readbackPaths[] = {
        "../GlesGpu/gpuVramReadback.inc",
        "WiiSXRX_2022/GlesGpu/gpuVramReadback.inc",
        "GlesGpu/gpuVramReadback.inc"
    };
    int i, pluginOpened = 0, pluginOrdered = 0;
    int readbackOpened = 0, readOnly = 0, resetWired = 0;

    for (i = 0; i < 3; i++)
    {
        char *buf = read_whole_file(pluginPaths[i]);
        char *finish, *finishEnd, *mark, *flow, *probe, *upload, *check;
        char *diagBegin, *diagEnd;

        if (buf == NULL)
            continue;
        pluginOpened = 1;
        finish = strstr(buf, "static inline void FinishedVRAMWrite(void)");
        finishEnd = finish != NULL ?
                    strstr(finish + 1, "\n__inline void FinishedVRAMRead") :
                    NULL;
        mark = finish != NULL ? strstr(finish, "MarkCpuVramWriteKind(") : NULL;
        flow = mark != NULL ?
               strstr(mark, "T6CpuWriteProvenanceUpdateA0Flow(") : NULL;
        probe = flow != NULL ? strstr(flow, "T6CpuNewerProbeForA0(") : NULL;
        upload = probe != NULL ? strstr(probe, "if(bNeedWriteUpload)") : NULL;
        check = upload != NULL ? strstr(upload, "CheckWriteUpdate();") : NULL;
        diagBegin = mark != NULL ?
                    strstr(mark, "#if T6_BARRIER_DIAG") : NULL;
        /* Require the probe between the matching local DIAG boundaries and
         * before the synchronous upload, not merely somewhere in the TU. */
        diagEnd = probe != NULL ? strstr(probe, "#endif") : NULL;
        if (finish != NULL && finishEnd != NULL && mark != NULL &&
            flow != NULL && probe != NULL && upload != NULL && check != NULL &&
            diagBegin != NULL && diagEnd != NULL &&
            mark < diagBegin && diagBegin < flow && flow < probe &&
            probe < diagEnd && diagEnd < upload && upload < check &&
            check < finishEnd &&
            count_text_in_range(flow, probe, "isLogFileEnabled()") == 1 &&
            strstr(buf, "T6_CPU_NEWER_PROBE_MAX_RELEVANT = 64") != NULL &&
            strstr(buf, "T6_CPU_NEWER_PROBE_PASS_TARGET = 4") != NULL &&
            strstr(buf, "TRB CPU NEWER relevant=") != NULL)
            pluginOrdered = 1;
        free(buf);
        if (pluginOrdered)
            break;
    }

    for (i = 0; i < 3; i++)
    {
        char *buf = read_whole_file(readbackPaths[i]);
        char *start, *end, *shadow;

        if (buf == NULL)
            continue;
        readbackOpened = 1;
        start = strstr(buf, "static int T6CpuNewerProbeForA0(");
        end = start != NULL ? strstr(start, "\n}\n#endif") : NULL;
        shadow = start != NULL ?
                 strstr(start, "T6VramMoveShadowDecisionForRect(") : NULL;
        if (start != NULL && end != NULL && shadow != NULL && shadow < end &&
            count_text_in_range(start, end,
                                "T6VramMoveShadowDecisionForRect(") == 1 &&
            count_text_in_range(start, end, "CaptureEfbSnapshot") == 0 &&
            count_text_in_range(start, end, "MergeReadback") == 0 &&
            count_text_in_range(start, end, "Invalidate") == 0 &&
            count_text_in_range(start, end, "MarkCpuVramWrite") == 0 &&
            count_text_in_range(start, end, "CheckWriteUpdate") == 0)
            readOnly = 1;
        {
            char *reset = strstr(buf,
                "static void ResetVramReadbackState(void)");

            if (reset != NULL &&
                strstr(reset, "g_t6CpuNewerProbeRelevant = 0;") != NULL &&
                strstr(reset, "g_t6CpuNewerProbeCandidates = 0;") != NULL &&
                strstr(reset, "g_t6CpuNewerProbePasses = 0;") != NULL)
                resetWired = 1;
        }
        free(buf);
        if (readOnly && resetWired)
            break;
    }

    expect_bool(pluginOpened, 1, "gpuPlugin.c opened for CPU-newer probe");
    expect_bool(pluginOrdered, 1,
                "A0 epoch < CPU-newer DIAG probe < synchronous upload");
    expect_bool(readbackOpened, 1,
                "gpuVramReadback.inc opened for CPU-newer probe");
    expect_bool(readOnly, 1,
                "CPU-newer probe has one shadow call and no write primitive");
    expect_bool(resetWired, 1,
                "CPU-newer probe counters reset with readback session");
}

int main(void)
{
    test_dc2_readback_scope();
    test_epoch();
    test_vram_write_payload_gate();
    test_potential_tile_prefilter();
    test_append();
    test_source_linear();
    test_standard_word_rect();
    test_standard_dependency();
    test_window_dependency();
    test_move_dependency();
    test_freshness();
    test_cpu_newer_probe_qualification();
    test_tile_enumeration();
    test_interleaved_swizzle_coverage();
    test_mixed_dependency();
    test_clut_boundary();
    test_builder_invalid();
    test_capacity_injection();
    test_t6_materialize_decision();
    test_t6_snapshot_selection();
    test_t6_merge_pixel();
    test_rgb5a3_tile_sample_plan();
    test_t6_c0_cross();
    test_filtered_resolution_eligibility();
    test_t6_plan();
    test_materialize_core_repeat();
    test_materialize_core_fail_no_change();
    test_materialize_core_slot_replacement();
    test_materialize_core_baseline();
    test_materialize_core_changed_runs();
    test_map_classification();
    test_physical_ownership();
    test_c0_hash_equivalence();
    test_rebuild_baseline_gate();
    test_session_compact_gates();
    test_window_lookup_source_only();
    test_window_lookup_clut4_order();
    test_window_lookup_clut8_order();
    test_window_lookup_preset_hit();
    test_window_lookup_preset_miss();
    test_window_lookup_stale_hit_source_capture();
    test_window_lookup_stale_hit_clut_capture();
    test_production_checksum_vector();
    test_capture_failure_reason();
    test_window_lookup_harness_cumulative_capture();
    test_capture_diag_production_flow();
    test_copytex_stats_shared();
    test_standard_lookup_cache_miss();
    test_standard_lookup_preset_hit();
    test_standard_lookup_clut4_palette();
    test_standard_lookup_clut8_palette();
    test_standard_lookup_capture_suppression();
    test_standard_lookup_identity();
    test_standard_lookup_stale_hit_source();
    test_standard_lookup_stale_hit_clut4();
    test_standard_lookup_stale_hit_clut8();
    test_standard_lookup_stale_hit_interleaved_mode0();
    test_standard_lookup_stale_hit_interleaved_mode1();
    test_standard_page_invalidate_core();
    test_standard_lookup_same_color_interleaved_mode0();
    test_standard_lookup_same_color_interleaved_mode1();
    test_standard_lookup_stale_hit_interleaved_page15_wrap();
    test_standard_partition_sweep_integration();
    test_read_fresh_early_gate();
    test_production_wiring_source();
    test_read_fresh_entry_wiring();
    test_materialize_commit_context_wiring();
    test_snapshot_potential_wiring();
    test_materialize_sample_plan_wiring();
    test_vram_write_payload_gate_wiring();
    test_filtered_resolution_promotion_wiring();
    test_move_shadow_full_decision();
    test_move_post_generic_regression();
    test_move_timing_delta();
    test_move_i_positive();
    test_move_i_negative();
    test_move_no_delta_second_read();
    test_move_copy_core_equivalence();
    test_move_diag_workspace_layout();
    test_move_source_freshness_precheck();
    test_move_takeover_source_order();
    test_cpu_newer_probe_source_order();
    test_move_shadow_comparator();
    test_move_shadow_overflow_injection();
    test_move_evidence_tile_count_matches();
    test_move_shadow_contract();
    test_move_required_consistency();
    test_move_required_mapid_gate();
    test_move_decision_count_result_gate();
    test_move_evidence_relational();
    test_move_finalize_end_to_end();
    test_move_full_tile_resolution();
    test_move_capture_preflight_and_cpu_provenance();
    test_move_capture_preflight_source();
    test_move_compare_detail_kinds();

    if (s_failures == 0)
        printf("PASS t6_barrier\n");
    else
        printf("FAIL t6_barrier (%d)\n", s_failures);

    return s_failures == 0 ? 0 : 1;
}
