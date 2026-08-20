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

    if (s_failures == 0)
        printf("PASS t6_barrier\n");
    else
        printf("FAIL t6_barrier (%d)\n", s_failures);

    return s_failures == 0 ? 0 : 1;
}
