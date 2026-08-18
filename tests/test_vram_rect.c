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
    test_dispatch();
    test_reject();

    if (s_failures == 0)
        printf("PASS vram_rect\n");
    else
        printf("FAIL vram_rect (%d)\n", s_failures);

    return s_failures == 0 ? 0 : 1;
}
