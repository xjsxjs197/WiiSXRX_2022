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

    if (s_failures == 0)
        printf("PASS t6_barrier\n");
    else
        printf("FAIL t6_barrier (%d)\n", s_failures);

    return s_failures == 0 ? 0 : 1;
}
