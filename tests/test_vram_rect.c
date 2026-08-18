/***************************************************************************
                          test_vram_rect.c
                             -------------------
    Host-side tests for the GlesGpu half-open VRAM rectangle helper.
    Not part of the Wii build; compile with a host C compiler:
        cc -std=c99 -Wall -Wextra -Werror -I../GlesGpu \
           test_vram_rect.c -o test_vram_rect
 ***************************************************************************/

#include <stdio.h>
#include <string.h>

#include "../GlesGpu/gpuVramRect.h"

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

static void expect_intersects(int expected, const VramRect *a,
                              const VramRect *b)
{
    int got = VramRectIntersects(a, b);

    if (got != expected)
    {
        printf("FAIL intersects got %d expected %d\n", got, expected);
        s_failures++;
    }
}

static void expect_depends(int expected, unsigned int storedClutId,
                           int pageid, int textureMode, const VramRect *rect)
{
    int got = TextureWindowEntryDependsOnRect(storedClutId, pageid,
                                              textureMode, 0x7FFF, 0x1FF,
                                              1024, 512, rect);

    if (got != expected)
    {
        printf("FAIL depends got %d expected %d\n", got, expected);
        s_failures++;
    }
}

static void test_basic(void)
{
    VramRect out[4];
    int n;

    n = SplitWrappedVramRect(10, 20, 5, 7, 1024, 512, out);
    expect_count(n, 1);
    if (n > 0)
        expect_rect(&out[0], 10, 20, 15, 27);

    n = SplitWrappedVramRect(1023, 511, 1, 1, 1024, 512, out);
    expect_count(n, 1);
    if (n > 0)
        expect_rect(&out[0], 1023, 511, 1024, 512);
}

static void test_x_wrap(void)
{
    VramRect out[4];
    int n;

    n = SplitWrappedVramRect(1022, 10, 4, 2, 1024, 512, out);
    expect_count(n, 2);
    if (n >= 1)
        expect_rect(&out[0], 1022, 10, 1024, 12);
    if (n >= 2)
        expect_rect(&out[1], 0, 10, 2, 12);
}

static void test_y_wrap(void)
{
    VramRect out[4];
    int n;

    n = SplitWrappedVramRect(10, 510, 2, 4, 1024, 512, out);
    expect_count(n, 2);
    if (n >= 1)
        expect_rect(&out[0], 10, 510, 12, 512);
    if (n >= 2)
        expect_rect(&out[1], 10, 0, 12, 2);
}

static void test_xy_wrap(void)
{
    VramRect out[4];
    int n;

    n = SplitWrappedVramRect(1022, 510, 4, 4, 1024, 512, out);
    expect_count(n, 4);
    if (n >= 1)
        expect_rect(&out[0], 1022, 510, 1024, 512);
    if (n >= 2)
        expect_rect(&out[1], 0, 510, 2, 512);
    if (n >= 3)
        expect_rect(&out[2], 1022, 0, 1024, 2);
    if (n >= 4)
        expect_rect(&out[3], 0, 0, 2, 2);
}

static void test_full_coverage(void)
{
    VramRect out[4];
    int n;

    n = SplitWrappedVramRect(100, 0, 1024, 512, 1024, 512, out);
    expect_count(n, 1);
    if (n > 0)
        expect_rect(&out[0], 0, 0, 1024, 512);

    n = SplitWrappedVramRect(100, 500, 2000, 30, 1024, 512, out);
    expect_count(n, 2);
    if (n >= 1)
        expect_rect(&out[0], 0, 500, 1024, 512);
    if (n >= 2)
        expect_rect(&out[1], 0, 0, 1024, 18);
}

static void test_thin_rects(void)
{
    VramRect out[4];
    int n;

    n = SplitWrappedVramRect(10, 20, 1, 5, 1024, 512, out);
    expect_count(n, 1);
    if (n > 0)
        expect_rect(&out[0], 10, 20, 11, 25);

    n = SplitWrappedVramRect(10, 20, 5, 1, 1024, 512, out);
    expect_count(n, 1);
    if (n > 0)
        expect_rect(&out[0], 10, 20, 15, 21);

    n = SplitWrappedVramRect(1023, 20, 1, 5, 1024, 512, out);
    expect_count(n, 1);
    if (n > 0)
        expect_rect(&out[0], 1023, 20, 1024, 25);

    n = SplitWrappedVramRect(10, 511, 5, 1, 1024, 512, out);
    expect_count(n, 1);
    if (n > 0)
        expect_rect(&out[0], 10, 511, 15, 512);
}

static void test_height_1024(void)
{
    VramRect out[4];
    int n;

    n = SplitWrappedVramRect(10, 1022, 2, 4, 1024, 1024, out);
    expect_count(n, 2);
    if (n >= 1)
        expect_rect(&out[0], 10, 1022, 12, 1024);
    if (n >= 2)
        expect_rect(&out[1], 10, 0, 12, 2);

    n = SplitWrappedVramRect(1022, 1022, 4, 4, 1024, 1024, out);
    expect_count(n, 4);
    if (n >= 1)
        expect_rect(&out[0], 1022, 1022, 1024, 1024);
    if (n >= 2)
        expect_rect(&out[1], 0, 1022, 2, 1024);
    if (n >= 3)
        expect_rect(&out[2], 1022, 0, 1024, 2);
    if (n >= 4)
        expect_rect(&out[3], 0, 0, 2, 2);
}

static void test_window_footprint(void)
{
    VramRect out[4];
    VramRect write;
    int n;

    n = TextureWindowSourceRects(0, 0, 1024, 512, out);
    expect_count(n, 1);
    if (n > 0)
        expect_rect(&out[0], 0, 0, 64, 256);

    n = TextureWindowSourceRects(0, 1, 1024, 512, out);
    expect_count(n, 1);
    if (n > 0)
        expect_rect(&out[0], 0, 0, 128, 256);

    n = TextureWindowSourceRects(0, 2, 1024, 512, out);
    expect_count(n, 1);
    if (n > 0)
        expect_rect(&out[0], 0, 0, 256, 256);

    n = TextureWindowSourceRects(17, 1, 1024, 512, out);
    expect_count(n, 1);
    if (n > 0)
        expect_rect(&out[0], 64, 256, 192, 512);

    /* mode 0 page 0: only the first 64-word segment depends on it. */
    write = (VramRect){ 64, 0, 128, 256 };
    n = TextureWindowSourceRects(0, 0, 1024, 512, out);
    expect_count(n, 1);
    if (n > 0)
        expect_intersects(0, &write, &out[0]);

    /* mode 1 page 0: second 64-word segment still belongs to the page. */
    write = (VramRect){ 64, 0, 128, 256 };
    n = TextureWindowSourceRects(0, 1, 1024, 512, out);
    expect_count(n, 1);
    if (n > 0)
        expect_intersects(1, &write, &out[0]);
    write = (VramRect){ 128, 0, 192, 256 };
    if (n > 0)
        expect_intersects(0, &write, &out[0]);

    /* mode 2 page 0: segments 2..4 belong to the page, segment 5 does not. */
    n = TextureWindowSourceRects(0, 2, 1024, 512, out);
    expect_count(n, 1);
    write = (VramRect){ 128, 0, 192, 256 };
    if (n > 0)
        expect_intersects(1, &write, &out[0]);
    write = (VramRect){ 192, 0, 256, 256 };
    if (n > 0)
        expect_intersects(1, &write, &out[0]);
    write = (VramRect){ 256, 0, 320, 256 };
    if (n > 0)
        expect_intersects(0, &write, &out[0]);
}

static void test_window_palette(void)
{
    VramRect pal[4];
    VramRect write;
    int n;

    n = TextureWindowPaletteRects(0, 0, 0x1FF, 1024, 512, pal);
    expect_count(n, 1);
    if (n > 0)
        expect_rect(&pal[0], 0, 0, 16, 1);

    n = TextureWindowPaletteRects(0, 1, 0x1FF, 1024, 512, pal);
    expect_count(n, 1);
    if (n > 0)
        expect_rect(&pal[0], 0, 0, 256, 1);

    /* clutId with cy=1: palette starts on VRAM row 1. */
    n = TextureWindowPaletteRects(64, 0, 0x1FF, 1024, 512, pal);
    expect_count(n, 1);
    if (n > 0)
        expect_rect(&pal[0], 0, 1, 16, 2);

    /* 8-bit palette near the right edge wraps to the left side. */
    n = TextureWindowPaletteRects(0x3F, 1, 0x1FF, 1024, 512, pal);
    expect_count(n, 2);
    if (n >= 1)
        expect_rect(&pal[0], 1008, 0, 1024, 1);
    if (n >= 2)
        expect_rect(&pal[1], 0, 0, 240, 1);

    /* mode 0 palette is exactly 16 words; word 16 is not a dependency. */
    n = TextureWindowPaletteRects(0, 0, 0x1FF, 1024, 512, pal);
    write = (VramRect){ 15, 0, 16, 1 };
    expect_intersects(1, &write, &pal[0]);
    write = (VramRect){ 16, 0, 17, 1 };
    expect_intersects(0, &write, &pal[0]);

    /* mode 1 palette is exactly 256 words; word 256 is not a dependency. */
    n = TextureWindowPaletteRects(0, 1, 0x1FF, 1024, 512, pal);
    write = (VramRect){ 255, 0, 256, 1 };
    expect_intersects(1, &write, &pal[0]);
    write = (VramRect){ 256, 0, 257, 1 };
    expect_intersects(0, &write, &pal[0]);
}

static void test_window_source_right_edge(void)
{
    VramRect out[4];
    VramRect write;
    int n;

    /* page 15, mode 1: [960,1024) plus wrapped [0,64). */
    n = TextureWindowSourceRects(15, 1, 1024, 512, out);
    expect_count(n, 2);
    if (n >= 1)
        expect_rect(&out[0], 960, 0, 1024, 256);
    if (n >= 2)
        expect_rect(&out[1], 0, 0, 64, 256);
    write = (VramRect){ 0, 0, 16, 1 };
    if (n >= 2)
        expect_intersects(1, &write, &out[1]);
    write = (VramRect){ 64, 0, 128, 1 };
    if (n >= 2)
        expect_intersects(0, &write, &out[1]);

    n = TextureWindowSourceLinearRects(15, 1, 1024, 512, out);
    expect_count(n, 2);
    if (n >= 1)
        expect_rect(&out[0], 960, 0, 1024, 256);
    if (n >= 2)
        expect_rect(&out[1], 0, 1, 64, 257);

    write = (VramRect){ 0, 1, 16, 2 };
    if (n >= 2)
        expect_intersects(1, &write, &out[1]);
    write = (VramRect){ 64, 1, 128, 2 };
    if (n >= 2)
        expect_intersects(0, &write, &out[1]);

    /* page 14, mode 2: [896,1024) plus wrapped [0,128). */
    n = TextureWindowSourceRects(14, 2, 1024, 512, out);
    expect_count(n, 2);
    if (n >= 1)
        expect_rect(&out[0], 896, 0, 1024, 256);
    if (n >= 2)
        expect_rect(&out[1], 0, 0, 128, 256);
    write = (VramRect){ 0, 0, 64, 1 };
    if (n >= 2)
        expect_intersects(1, &write, &out[1]);

    n = TextureWindowSourceLinearRects(14, 2, 1024, 512, out);
    expect_count(n, 2);
    if (n >= 1)
        expect_rect(&out[0], 896, 0, 1024, 256);
    if (n >= 2)
        expect_rect(&out[1], 0, 1, 128, 257);

    write = (VramRect){ 0, 1, 64, 2 };
    if (n >= 2)
        expect_intersects(1, &write, &out[1]);
    write = (VramRect){ 128, 1, 192, 2 };
    if (n >= 2)
        expect_intersects(0, &write, &out[1]);
}

static void test_window_palette_right_edge(void)
{
    VramRect pal[4];
    VramRect write;
    int n;

    /* clut 0x3F with checksum/semi bits: row-local [1008,1024)+[0,240). */
    n = TextureWindowPaletteRects(0x3F, 1, 0x1FF, 1024, 512, pal);
    expect_count(n, 2);
    if (n >= 1)
        expect_rect(&pal[0], 1008, 0, 1024, 1);
    if (n >= 2)
        expect_rect(&pal[1], 0, 0, 240, 1);
    write = (VramRect){ 0, 0, 16, 1 };
    if (n >= 2)
        expect_intersects(1, &write, &pal[1]);
    write = (VramRect){ 240, 0, 256, 1 };
    if (n >= 2)
        expect_intersects(0, &write, &pal[1]);

    n = TextureWindowPaletteLinearRects(0x3F, 1, 0x1FF, 1024, 512, pal);
    expect_count(n, 2);
    if (n >= 1)
        expect_rect(&pal[0], 1008, 0, 1024, 1);
    if (n >= 2)
        expect_rect(&pal[1], 0, 1, 240, 2);

    write = (VramRect){ 0, 1, 16, 2 };
    if (n >= 2)
        expect_intersects(1, &write, &pal[1]);
    write = (VramRect){ 240, 1, 256, 2 };
    if (n >= 2)
        expect_intersects(0, &write, &pal[1]);
}

static void test_entry_dependency(void)
{
    VramRect write;
    unsigned int clutA = 0x40u;
    unsigned int clutB = 0x40000000u | (0x3ABCu << 16) | 0x20u;

    /* Entry A: page 16 (y page 1), mode 0, CLUT 0x40 (palette row 1). */
    /* Entry B: page 8, mode 0, CLUT 0x20 with checksum/semi bits. */
    /* Entry C: page 15, mode 2, no CLUT (right-edge source). */

    write = (VramRect){ 0, 0, 16, 1 };
    expect_depends(0, clutA, 16, 0, &write); /* A untouched */
    expect_depends(0, clutB, 8, 0, &write);  /* B untouched */
    expect_depends(1, 0, 15, 2, &write);     /* C wrapped source */

    write = (VramRect){ 512, 0, 576, 1 };
    expect_depends(0, clutA, 16, 0, &write); /* A untouched */
    expect_depends(1, clutB, 8, 0, &write);  /* B source */
    expect_depends(0, 0, 15, 2, &write);     /* C untouched */

    write = (VramRect){ 960, 0, 976, 1 };
    expect_depends(0, clutA, 16, 0, &write); /* A untouched */
    expect_depends(0, clutB, 8, 0, &write);  /* B untouched */
    expect_depends(1, 0, 15, 2, &write);     /* C right source piece */

    write = (VramRect){ 0, 1, 16, 2 };
    expect_depends(1, clutA, 16, 0, &write); /* A palette row 1 */
    expect_depends(0, clutB, 8, 0, &write);  /* B untouched */
    expect_depends(1, 0, 15, 2, &write);     /* C linear spill row 1 */

    write = (VramRect){ 576, 0, 592, 1 };
    expect_depends(0, clutA, 16, 0, &write); /* A untouched */
    expect_depends(0, clutB, 8, 0, &write);  /* B untouched */
    expect_depends(0, 0, 15, 2, &write);     /* C untouched */
}

typedef struct DispatchRecord
{
    VramRect rects[8];
    int count;
} DispatchRecord;

static void record_rect(void *user_data, const VramRect *rect)
{
    DispatchRecord *rec = (DispatchRecord *)user_data;

    if (rec->count < 8)
        rec->rects[rec->count++] = *rect;
}

static void test_dispatch(void)
{
    DispatchRecord rec;
    int n;

    memset(&rec, 0, sizeof(rec));
    n = ForEachWrappedVramRect(-3, 10, 8, 4, 1024, 512,
                               record_rect, &rec);
    expect_count(n, 1);
    expect_count(rec.count, 1);
    if (rec.count > 0)
        expect_rect(&rec.rects[0], 0, 10, 5, 14);

    memset(&rec, 0, sizeof(rec));
    n = ForEachWrappedVramRect(1022, 510, 4, 4, 1024, 512,
                               record_rect, &rec);
    expect_count(n, 4);
    expect_count(rec.count, 4);

    memset(&rec, 0, sizeof(rec));
    n = ForEachWrappedVramRect(10, 20, 0, 5, 1024, 512,
                               record_rect, &rec);
    expect_count(n, 0);
    expect_count(rec.count, 0);
}

static void test_reject(void)
{
    VramRect out[4];

    expect_count(SplitWrappedVramRect(10, 10, 0, 5, 1024, 512, out), 0);
    expect_count(SplitWrappedVramRect(10, 10, 5, 0, 1024, 512, out), 0);
    expect_count(SplitWrappedVramRect(10, 10, -1, 5, 1024, 512, out), 0);
    expect_count(SplitWrappedVramRect(-1, 10, 5, 5, 1024, 512, out), 0);
    expect_count(SplitWrappedVramRect(1024, 10, 5, 5, 1024, 512, out), 0);
    expect_count(SplitWrappedVramRect(10, 512, 5, 5, 1024, 512, out), 0);
    expect_count(SplitWrappedVramRect(10, 10, 5, 5, 1024, 512, NULL), 0);
}

int main(void)
{
    test_basic();
    test_x_wrap();
    test_y_wrap();
    test_xy_wrap();
    test_full_coverage();
    test_thin_rects();
    test_height_1024();
    test_window_footprint();
    test_window_palette();
    test_window_source_right_edge();
    test_window_palette_right_edge();
    test_entry_dependency();
    test_dispatch();
    test_reject();

    if (s_failures == 0)
        printf("PASS vram_rect\n");
    else
        printf("FAIL vram_rect (%d)\n", s_failures);

    return s_failures == 0 ? 0 : 1;
}
