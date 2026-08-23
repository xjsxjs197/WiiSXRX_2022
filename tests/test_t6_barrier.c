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

static int s_failures = 0;

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

int main(void)
{
    test_epoch();
    test_append();
    test_source_linear();
    test_standard_word_rect();
    test_standard_dependency();
    test_window_dependency();
    test_move_dependency();
    test_freshness();
    test_tile_enumeration();
    test_interleaved_swizzle_coverage();
    test_mixed_dependency();
    test_clut_boundary();
    test_builder_invalid();
    test_capacity_injection();
    test_t6_materialize_decision();
    test_t6_snapshot_selection();
    test_t6_merge_pixel();
    test_t6_c0_cross();
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

    if (s_failures == 0)
        printf("PASS t6_barrier\n");
    else
        printf("FAIL t6_barrier (%d)\n", s_failures);

    return s_failures == 0 ? 0 : 1;
}
