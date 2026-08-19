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
#include "../GlesGpu/gpuSubTextureCache.h"

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

static void expect_sub_palette_depends(int expected, unsigned int storedClutId,
                                       int textureMode, const VramRect *rect)
{
    int got = StandardSubTexturePaletteDependsOnRect(storedClutId, textureMode,
                                                     0x7FFF, 0x1FF,
                                                     1024, 512, rect);

    if (got != expected)
    {
        printf("FAIL sub palette depends got %d expected %d\n",
               got, expected);
        s_failures++;
    }
}

static void expect_u32(unsigned int got, unsigned int expected, const char *what)
{
    if (got != expected)
    {
        printf("FAIL %s got %u expected %u\n", what, got, expected);
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

static unsigned int build_standard_clut_id(unsigned int low, unsigned int checksum)
{
    return (low & 0x7FFFu) | (checksum << 16) | 0x80000000u;
}

static int lookup_hits(const unsigned int *entries, int count,
                       unsigned int clutId)
{
    int i;

    for (i = 0; i < count; i++)
        if (entries[i] == clutId)
            return 1;

    return 0;
}

typedef struct InvalidateObserve
{
    int calls;
    unsigned int *target;
} InvalidateObserve;

static void observe_invalidate(void *user, void *entry)
{
    InvalidateObserve *obs = (InvalidateObserve *)user;

    obs->calls++;
    obs->target = (unsigned int *)entry;
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

static void test_subtexture_palette(void)
{
    VramRect write;
    unsigned int clutA;
    unsigned int clutB;

    /* 4-bit CLUT at x=0, y=0: words [0,16) on row 0. */
    write = (VramRect){ 0, 0, 1, 1 };
    expect_sub_palette_depends(1, 0, 0, &write);
    write = (VramRect){ 15, 0, 16, 1 };
    expect_sub_palette_depends(1, 0, 0, &write);
    write = (VramRect){ 16, 0, 17, 1 };
    expect_sub_palette_depends(0, 0, 0, &write);

    /* 8-bit CLUT at x=0, y=0: words [0,256) on row 0. */
    write = (VramRect){ 255, 0, 256, 1 };
    expect_sub_palette_depends(1, 0, 1, &write);
    write = (VramRect){ 256, 0, 257, 1 };
    expect_sub_palette_depends(0, 0, 1, &write);

    /* 8-bit CLUT starting at x=1008 wraps to [0,240) on the same row. */
    write = (VramRect){ 1008, 0, 1009, 1 };
    expect_sub_palette_depends(1, 0x3F, 1, &write);
    write = (VramRect){ 0, 0, 16, 1 };
    expect_sub_palette_depends(1, 0x3F, 1, &write);
    write = (VramRect){ 240, 0, 256, 1 };
    expect_sub_palette_depends(0, 0x3F, 1, &write);

    /* A CLUT on row 1 is only invalidated by writes to row 1. */
    write = (VramRect){ 0, 1, 16, 2 };
    expect_sub_palette_depends(1, 64, 0, &write);
    write = (VramRect){ 0, 0, 16, 1 };
    expect_sub_palette_depends(0, 64, 0, &write);

    /* Mode 2 has no CLUT, so no palette dependency. */
    write = (VramRect){ 0, 0, 256, 1 };
    expect_sub_palette_depends(0, 0, 2, &write);

    /* The checksum/semi-transparency bits are not part of the dependency. */
    clutA = 0x20u;
    clutB = 0x80000000u | (0x3ABCu << 16) | 0x20u;
    write = (VramRect){ 512, 0, 513, 1 };
    expect_sub_palette_depends(1, clutA, 0, &write);
    expect_sub_palette_depends(1, clutB, 0, &write);
    write = (VramRect){ 528, 0, 529, 1 };
    expect_sub_palette_depends(0, clutA, 0, &write);
    expect_sub_palette_depends(0, clutB, 0, &write);
}

static void test_checksum_collision(void)
{
    uint16_t pal4a[16] = {0};
    uint16_t pal4b[16] = {0};
    uint16_t pal8a[256] = {0};
    uint16_t pal8b[256] = {0};
    unsigned int c4a, c4b, c8a, c8b;

    pal4b[15] = 64;
    c4a = SubTexturePaletteChecksum14(pal4a, 0);
    c4b = SubTexturePaletteChecksum14(pal4b, 0);
    expect_u32(c4b, c4a, "4bit checksum collision");

    pal8b[255] = 128;
    c8a = SubTexturePaletteChecksum14(pal8a, 1);
    c8b = SubTexturePaletteChecksum14(pal8b, 1);
    expect_u32(c8b, c8a, "8bit checksum collision");
}

static void test_palette_invalidation_lookup(void)
{
    uint16_t palA[16] = {0};
    uint16_t palB[16] = {0};
    unsigned int entries[2];
    InvalidateObserve obs;
    VramRect write;
    unsigned int targetClut;
    unsigned int unrelatedClut;
    unsigned int checksum;
    int invalidated;

    palB[15] = 64;
    checksum = SubTexturePaletteChecksum14(palA, 0);
    expect_u32(SubTexturePaletteChecksum14(palB, 0), checksum,
               "lookup collision checksum");
    targetClut = build_standard_clut_id(0x00, checksum);
    unrelatedClut = build_standard_clut_id(0x40, 1000);

    /* Writing the first CLUT word clears the dependent entry. */
    memset(&entries, 0, sizeof(entries));
    entries[0] = targetClut;
    entries[1] = unrelatedClut;
    write = (VramRect){ 0, 0, 1, 1 };
    memset(&obs, 0, sizeof(obs));
    invalidated = SubTexturePaletteInvalidateEntries(
        entries, 2, sizeof(entries[0]), 0, 0x7FFF, 0x1FF, 1024, 512,
        &write, 1, observe_invalidate, &obs);
    expect_count(invalidated, 1);
    expect_count(obs.calls, 1);
    expect_bool(obs.target == &entries[0], 1, "target first-word callback");
    expect_bool(entries[0] == 0, 1, "target first-word cleared");
    expect_bool(entries[1] == unrelatedClut, 1,
                "unrelated first-word kept");

    /* Writing the last CLUT word clears it as well. */
    memset(&entries, 0, sizeof(entries));
    entries[0] = targetClut;
    entries[1] = unrelatedClut;
    write = (VramRect){ 15, 0, 16, 1 };
    memset(&obs, 0, sizeof(obs));
    invalidated = SubTexturePaletteInvalidateEntries(
        entries, 2, sizeof(entries[0]), 0, 0x7FFF, 0x1FF, 1024, 512,
        &write, 1, observe_invalidate, &obs);
    expect_count(invalidated, 1);
    expect_bool(entries[0] == 0, 1, "target last-word cleared");
    expect_bool(entries[1] == unrelatedClut, 1,
                "unrelated last-word kept");

    /* A cleared entry no longer matches a lookup; the unrelated one still does. */
    memset(&entries, 0, sizeof(entries));
    entries[0] = targetClut;
    entries[1] = unrelatedClut;
    write = (VramRect){ 0, 0, 16, 1 };
    memset(&obs, 0, sizeof(obs));
    invalidated = SubTexturePaletteInvalidateEntries(
        entries, 2, sizeof(entries[0]), 0, 0x7FFF, 0x1FF, 1024, 512,
        &write, 1, observe_invalidate, &obs);

    expect_count(invalidated, 1);
    expect_bool(lookup_hits(entries, 2, targetClut) == 0, 1,
                "target lookup miss");
    expect_bool(lookup_hits(entries, 2, unrelatedClut) == 1, 1,
                "unrelated lookup hit");
}

static void test_palette_invalidation_8bit_wrap(void)
{
    unsigned int entry;
    InvalidateObserve obs;
    VramRect write;
    int invalidated;

    entry = build_standard_clut_id(0x3F, 8127);
    write = (VramRect){ 0, 0, 16, 1 };
    memset(&obs, 0, sizeof(obs));
    invalidated = SubTexturePaletteInvalidateEntries(
        &entry, 1, sizeof(entry), 1, 0x7FFF, 0x1FF, 1024, 512,
        &write, 1, observe_invalidate, &obs);
    expect_count(invalidated, 1);
    expect_bool(entry == 0, 1, "8bit row-local wrap cleared");

    entry = build_standard_clut_id(0x3F, 8127);
    write = (VramRect){ 0, 1, 16, 2 };
    memset(&obs, 0, sizeof(obs));
    invalidated = SubTexturePaletteInvalidateEntries(
        &entry, 1, sizeof(entry), 1, 0x7FFF, 0x1FF, 1024, 512,
        &write, 1, observe_invalidate, &obs);
    expect_count(invalidated, 1);
    expect_bool(entry == 0, 1, "8bit linear spill cleared");

    entry = build_standard_clut_id(0x3F, 8127);
    write = (VramRect){ 240, 0, 256, 1 };
    memset(&obs, 0, sizeof(obs));
    invalidated = SubTexturePaletteInvalidateEntries(
        &entry, 1, sizeof(entry), 1, 0x7FFF, 0x1FF, 1024, 512,
        &write, 1, observe_invalidate, &obs);
    expect_count(invalidated, 0);
    expect_bool(entry != 0, 1, "8bit non-dependency kept");
}

typedef struct PaddedEntry
{
    unsigned int clut;
    unsigned int canary;
    unsigned char pad[16];
} PaddedEntry;

static void test_palette_invalidation_padded_stride(void)
{
    PaddedEntry entries[2];
    InvalidateObserve obs;
    VramRect write;
    unsigned int targetClut;
    unsigned int unrelatedClut;
    int invalidated;

    memset(&entries, 0xCC, sizeof(entries));

    targetClut = build_standard_clut_id(0x00, 15873);
    unrelatedClut = build_standard_clut_id(0x40, 1000);

    entries[0].clut = targetClut;
    entries[0].canary = 0xA5A5A5A5u;
    entries[1].clut = unrelatedClut;
    entries[1].canary = 0x5A5A5A5Au;

    write = (VramRect){ 0, 0, 16, 1 };
    memset(&obs, 0, sizeof(obs));
    invalidated = SubTexturePaletteInvalidateEntries(
        entries, 2, sizeof(entries[0]), 0, 0x7FFF, 0x1FF, 1024, 512,
        &write, 1, observe_invalidate, &obs);

    expect_count(invalidated, 1);
    expect_count(obs.calls, 1);
    expect_bool(obs.target == &entries[0].clut, 1, "padded target callback");
    expect_bool(entries[0].clut == 0, 1, "padded target cleared");
    expect_bool(entries[0].canary == 0xA5A5A5A5u, 1, "padded canary kept");
    expect_bool(entries[1].clut == unrelatedClut, 1, "padded unrelated kept");
    expect_bool(entries[1].canary == 0x5A5A5A5Au, 1,
                "padded unrelated canary kept");
}

static void test_palette_invalidation_reject_stride(void)
{
    unsigned int entry = build_standard_clut_id(0x00, 15873);
    VramRect write = (VramRect){ 0, 0, 16, 1 };
    int invalidated;

    invalidated = SubTexturePaletteInvalidateEntries(
        &entry, 1, sizeof(unsigned int) - 1, 0, 0x7FFF, 0x1FF, 1024, 512,
        &write, 1, NULL, NULL);

    expect_count(invalidated, 0);
    expect_bool(entry != 0, 1, "invalid stride entry kept");
}

typedef struct TileRunRecord
{
    VramRect rects[32];
    int count;
} TileRunRecord;

static void record_tile_run(void *user, int x0, int y0, int x1, int y1)
{
    TileRunRecord *rec = (TileRunRecord *)user;

    if (rec->count < 32)
    {
        rec->rects[rec->count].x0 = x0;
        rec->rects[rec->count].y0 = y0;
        rec->rects[rec->count].x1 = x1;
        rec->rects[rec->count].y1 = y1;
        rec->count++;
    }
}

static void test_tile_run_merge(void)
{
    unsigned char grid[32][64];
    TileRunRecord rec;
    int n, ty, tx;

    memset(grid, 0, sizeof(grid));
    for (ty = 0; ty < 15; ty++)
        for (tx = 0; tx < 20; tx++)
            grid[ty][tx] = 1;
    memset(&rec, 0, sizeof(rec));
    n = ForEachHorizontalTileRun(&grid[0][0], 64, 32, 16,
                                 record_tile_run, &rec);
    expect_count(n, 15);
    expect_count(rec.count, 15);
    if (rec.count > 0)
        expect_rect(&rec.rects[0], 0, 0, 320, 16);
    if (rec.count > 14)
        expect_rect(&rec.rects[14], 0, 224, 320, 240);

    memset(grid, 0, sizeof(grid));
    grid[0][0] = 1;
    grid[0][1] = 1;
    grid[0][3] = 1;
    memset(&rec, 0, sizeof(rec));
    n = ForEachHorizontalTileRun(&grid[0][0], 64, 32, 16,
                                 record_tile_run, &rec);
    expect_count(n, 2);
    expect_count(rec.count, 2);
    if (rec.count >= 2)
    {
        expect_rect(&rec.rects[0], 0, 0, 32, 16);
        expect_rect(&rec.rects[1], 48, 0, 64, 16);
    }

    memset(grid, 0, sizeof(grid));
    memset(&rec, 0, sizeof(rec));
    n = ForEachHorizontalTileRun(&grid[0][0], 64, 32, 16,
                                 record_tile_run, &rec);
    expect_count(n, 0);
    expect_count(rec.count, 0);

    memset(grid, 0, sizeof(grid));
    grid[0][63] = 1;
    grid[0][0] = 1;
    memset(&rec, 0, sizeof(rec));
    n = ForEachHorizontalTileRun(&grid[0][0], 64, 32, 16,
                                 record_tile_run, &rec);
    expect_count(n, 2);
    expect_count(rec.count, 2);
    if (rec.count >= 2)
    {
        expect_rect(&rec.rects[0], 0, 0, 16, 16);
        expect_rect(&rec.rects[1], 1008, 0, 1024, 16);
    }

    /* 640x480 full: 40x30 tiles -> 30 runs. */
    memset(grid, 0, sizeof(grid));
    for (ty = 0; ty < 30; ty++)
        for (tx = 0; tx < 40; tx++)
            grid[ty][tx] = 1;
    memset(&rec, 0, sizeof(rec));
    n = ForEachHorizontalTileRun(&grid[0][0], 64, 32, 16,
                                 record_tile_run, &rec);
    expect_count(n, 30);
    expect_count(rec.count, 30);
    if (rec.count > 0)
        expect_rect(&rec.rects[0], 0, 0, 640, 16);
    if (rec.count > 29)
        expect_rect(&rec.rects[29], 0, 464, 640, 480);

    /* Single tile. */
    memset(grid, 0, sizeof(grid));
    grid[0][0] = 1;
    memset(&rec, 0, sizeof(rec));
    n = ForEachHorizontalTileRun(&grid[0][0], 64, 32, 16,
                                 record_tile_run, &rec);
    expect_count(n, 1);
    expect_count(rec.count, 1);
    if (rec.count > 0)
        expect_rect(&rec.rects[0], 0, 0, 16, 16);

    /* Small continuous run. */
    memset(grid, 0, sizeof(grid));
    grid[0][1] = 1;
    grid[0][2] = 1;
    memset(&rec, 0, sizeof(rec));
    n = ForEachHorizontalTileRun(&grid[0][0], 64, 32, 16,
                                 record_tile_run, &rec);
    expect_count(n, 1);
    expect_count(rec.count, 1);
    if (rec.count > 0)
        expect_rect(&rec.rects[0], 16, 0, 48, 16);

    /* Sparse hole across rows. */
    memset(grid, 0, sizeof(grid));
    grid[0][0] = 1;
    grid[0][5] = 1;
    grid[1][3] = 1;
    memset(&rec, 0, sizeof(rec));
    n = ForEachHorizontalTileRun(&grid[0][0], 64, 32, 16,
                                 record_tile_run, &rec);
    expect_count(n, 3);
    expect_count(rec.count, 3);
    if (rec.count >= 3)
    {
        expect_rect(&rec.rects[0], 0, 0, 16, 16);
        expect_rect(&rec.rects[1], 80, 0, 96, 16);
        expect_rect(&rec.rects[2], 48, 16, 64, 32);
    }
}

static void fill_tile_coverage(const TileRunRecord *rec,
                               unsigned char cover[32][64])
{
    int i, ty, tx;

    memset(cover, 0, 32 * 64);
    for (i = 0; i < rec->count; i++)
    {
        int x0 = rec->rects[i].x0 / 16;
        int x1 = rec->rects[i].x1 / 16;
        int y0 = rec->rects[i].y0 / 16;
        int y1 = rec->rects[i].y1 / 16;

        for (ty = y0; ty < y1; ty++)
            for (tx = x0; tx < x1; tx++)
                cover[ty][tx] = 1;
    }
}

static void test_tile_run_coverage(void)
{
    unsigned char grid[32][64];
    unsigned char cover[32][64];
    TileRunRecord rec;
    int ty, tx;
    int ok;

    memset(grid, 0, sizeof(grid));
    for (ty = 0; ty < 15; ty++)
        for (tx = 0; tx < 20; tx++)
            grid[ty][tx] = 1;
    memset(&rec, 0, sizeof(rec));
    ForEachHorizontalTileRun(&grid[0][0], 64, 32, 16,
                             record_tile_run, &rec);
    fill_tile_coverage(&rec, cover);
    ok = memcmp(grid, cover, sizeof(grid)) == 0;
    expect_bool(ok, 1, "full run coverage equivalence");

    memset(grid, 0, sizeof(grid));
    grid[0][0] = 1;
    grid[0][1] = 1;
    grid[0][3] = 1;
    memset(&rec, 0, sizeof(rec));
    ForEachHorizontalTileRun(&grid[0][0], 64, 32, 16,
                             record_tile_run, &rec);
    fill_tile_coverage(&rec, cover);
    ok = memcmp(grid, cover, sizeof(grid)) == 0;
    expect_bool(ok, 1, "hole run coverage equivalence");

    memset(grid, 0, sizeof(grid));
    grid[0][63] = 1;
    grid[0][0] = 1;
    memset(&rec, 0, sizeof(rec));
    ForEachHorizontalTileRun(&grid[0][0], 64, 32, 16,
                             record_tile_run, &rec);
    fill_tile_coverage(&rec, cover);
    ok = memcmp(grid, cover, sizeof(grid)) == 0;
    expect_bool(ok, 1, "boundary run coverage equivalence");

    memset(grid, 0, sizeof(grid));
    for (ty = 0; ty < 30; ty++)
        for (tx = 0; tx < 40; tx++)
            grid[ty][tx] = 1;
    memset(&rec, 0, sizeof(rec));
    ForEachHorizontalTileRun(&grid[0][0], 64, 32, 16,
                             record_tile_run, &rec);
    fill_tile_coverage(&rec, cover);
    ok = memcmp(grid, cover, sizeof(grid)) == 0;
    expect_bool(ok, 1, "640x480 run coverage equivalence");
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
    test_subtexture_palette();
    test_checksum_collision();
    test_palette_invalidation_lookup();
    test_palette_invalidation_8bit_wrap();
    test_palette_invalidation_padded_stride();
    test_palette_invalidation_reject_stride();
    test_tile_run_merge();
    test_tile_run_coverage();
    test_entry_dependency();
    test_dispatch();
    test_reject();

    if (s_failures == 0)
        printf("PASS vram_rect\n");
    else
        printf("FAIL vram_rect (%d)\n", s_failures);

    return s_failures == 0 ? 0 : 1;
}
