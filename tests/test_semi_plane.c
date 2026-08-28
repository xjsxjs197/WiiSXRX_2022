/***************************************************************************
                          test_semi_plane.c
                             -------------------
    Host-side tests for the OpenGX semi-transparent plane conversion and
    reuse path.  Not part of the Wii build; compile with a host C compiler:
        cc -std=c99 -Wall -Wextra -Werror -I../deps/opengx \
           test_semi_plane.c -o test_semi_plane
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../deps/opengx/gxSemiPlane.h"
#include "../deps/opengx/gxTextureScramble.h"

static int s_failures = 0;
static unsigned int s_allocCount = 0;

static void expect_int(int got, int expected, const char *what)
{
    if (got != expected)
    {
        printf("FAIL %s got %d expected %d\n", what, got, expected);
        s_failures++;
    }
}

static void expect_flag(int value, int flag, int present, const char *what)
{
    int got = (value & flag) != 0;

    if (got != present)
    {
        printf("FAIL %s flag %d got %d expected %d\n",
               what, flag, got, present);
        s_failures++;
    }
}

static void expect_texel(const unsigned char *buf, int width,
                         int x, int y, unsigned short expected, const char *what)
{
    int blocksPerRow = W_BLOCK(width);
    int block = (y >> 2) * blocksPerRow + (x >> 2);
    int offset = block * 32 + ((y & 3) * 4 + (x & 3)) * 2;
    unsigned short got;

    memcpy(&got, buf + offset, sizeof(got));
    if (got != expected)
    {
        printf("FAIL %s at %d,%d got %04X expected %04X\n",
               what, x, y, got, expected);
        s_failures++;
    }
}

static void set_texel(unsigned char *src, int width,
                      int x, int y, unsigned short value)
{
    memcpy(src + (x + y * width) * 4 + 2, &value, sizeof(value));
}

static void fill_opaque(unsigned char *src, int width, int height,
                        unsigned short value)
{
    int i;

    for (i = 0; i < width * height; i++)
        memcpy(src + i * 4 + 2, &value, sizeof(value));
}

static void fill_semi(unsigned char *semi, int width, int height,
                      unsigned short value)
{
    int y, x;

    for (y = 0; y < height; y++)
        for (x = 0; x < width; x++)
        {
            int blocksPerRow = W_BLOCK(width);
            int block = (y >> 2) * blocksPerRow + (x >> 2);
            int offset = block * 32 + ((y & 3) * 4 + (x & 3)) * 2;
            memcpy(semi + offset, &value, sizeof(value));
        }
}

static void *alloc_tiled(unsigned int width, unsigned int height)
{
    void *p = malloc(GxRgb5a3TiledSize(width, height));

    if (p != NULL)
        s_allocCount++;
    return p;
}

/*
 * Mirrors the production glTexImage2D() semi-plane steps: shared prepare
 * decision, allocation only for the first/geometry-changing semi upload, and
 * the shared zero-init helper for a fresh plane.
 */
static int update_full(unsigned char *src, unsigned char *primary,
                       int width, int height,
                       GxSemiPlaneState *state,
                       GxSemiPlaneAction *actionOut, int *reinitOut)
{
    int hasSemi = GxScramble4b5a3HasSemi(src, width, height, 1);
    GxSemiPlaneAction action = GxSemiPlanePrepareFull(
        state, hasSemi, width, height, reinitOut);
    unsigned char *semiTarget = NULL;

    *actionOut = action;
    if (action == GX_SEMI_ACTION_ALLOC_ZERO_COPY)
    {
        state->data = alloc_tiled(width, height);
        GxSemiPlaneZero(state->data, width, height);
        state->size = GxRgb5a3TiledSize(width, height);
        semiTarget = state->data;
    }
    else if (action == GX_SEMI_ACTION_COPY)
    {
        semiTarget = state->data;
    }

    return GxScramble4b5a3Full(src, primary, semiTarget, 1, width, height);
}

static void test_abcd(void)
{
    const int w = 8;
    const int h = 8;
    unsigned char *src = malloc((size_t)w * h * 4);
    unsigned char *primary = malloc(GxRgb5a3TiledSize(w, h));
    GxSemiPlaneState state = { NULL, 0 };
    GxSemiPlaneAction action;
    int reinit = 0;
    int textureType;

    s_allocCount = 0;

    /* A: first upload with a semi texel -> allocate + zero + copy. */
    memset(src, 0, (size_t)w * h * 4);
    fill_opaque(src, w, h, 0x8001);
    set_texel(src, w, 1, 1, 0x0001);
    textureType = update_full(src, primary, w, h, &state, &action, &reinit);
    expect_int(action, GX_SEMI_ACTION_ALLOC_ZERO_COPY, "A action");
    expect_int(reinit, 1, "A reinit");
    expect_int(s_allocCount, 1, "A alloc");
    expect_flag(textureType, TEX_TYPE_1, 1, "A type1");
    expect_flag(textureType, TEX_TYPE_2, 1, "A type2");
    expect_texel(state.data, w, 1, 1, 0x8001, "A semi");
    expect_texel(state.data, w, 6, 2, 0x0000, "A semi other");
    expect_texel(primary, w, 1, 1, 0x0000, "A primary semi");
    expect_texel(primary, w, 6, 2, 0x8001, "A primary opaque");

    /* B: same-size reuse with a different semi texel -> copy only. */
    {
        void *semiBefore = state.data;
        memset(src, 0, (size_t)w * h * 4);
        fill_opaque(src, w, h, 0x8001);
        set_texel(src, w, 6, 2, 0x0002);
        textureType = update_full(src, primary, w, h, &state, &action, &reinit);
        expect_int(action, GX_SEMI_ACTION_COPY, "B action");
        expect_int(reinit, 0, "B reinit");
        expect_int(state.data == semiBefore, 1, "B reuse");
        expect_int(s_allocCount, 1, "B alloc");
        expect_flag(textureType, TEX_TYPE_1, 1, "B type1");
        expect_texel(state.data, w, 1, 1, 0x0000, "B old semi cleared");
        expect_texel(state.data, w, 6, 2, 0x8002, "B semi");
    }

    /* C: no semi texels -> retained plane untouched, no TEX_TYPE_1. */
    memset(src, 0, (size_t)w * h * 4);
    fill_opaque(src, w, h, 0x8001);
    textureType = update_full(src, primary, w, h, &state, &action, &reinit);
    expect_int(action, GX_SEMI_ACTION_NONE, "C action");
    expect_int(reinit, 0, "C reinit");
    expect_int(s_allocCount, 1, "C alloc");
    expect_flag(textureType, TEX_TYPE_1, 0, "C type1");
    expect_texel(state.data, w, 6, 2, 0x8002, "C retained semi");

    /* D: semi returns -> copy into retained plane. */
    memset(src, 0, (size_t)w * h * 4);
    fill_opaque(src, w, h, 0x8001);
    set_texel(src, w, 3, 3, 0x0003);
    textureType = update_full(src, primary, w, h, &state, &action, &reinit);
    expect_int(action, GX_SEMI_ACTION_COPY, "D action");
    expect_int(reinit, 0, "D reinit");
    expect_int(s_allocCount, 1, "D alloc");
    expect_flag(textureType, TEX_TYPE_1, 1, "D type1");
    expect_texel(state.data, w, 6, 2, 0x0000, "D old semi cleared");
    expect_texel(state.data, w, 3, 3, 0x8003, "D semi");

    free(state.data);
    free(primary);
    free(src);
}

static void test_canary(int width, int height)
{
    const int guard = 32;
    unsigned int tiled = GxRgb5a3TiledSize(width, height);
    unsigned char *src = malloc((size_t)width * height * 4);
    unsigned char *primary = malloc(tiled);
    unsigned char *guarded = malloc(tiled + guard * 2);
    unsigned char *semi = guarded + guard;
    int textureType;
    int i;

    memset(src, 0, (size_t)width * height * 4);
    fill_opaque(src, width, height, 0x8001);
    set_texel(src, width, 1, 1, 0x0001);
    memset(guarded, 0xA5, tiled + guard * 2);

    textureType = GxScramble4b5a3Full(src, primary, semi, 1, width, height);
    expect_flag(textureType, TEX_TYPE_1, 1, "canary type1");
    expect_flag(textureType, TEX_TYPE_2, 1, "canary type2");

    for (i = 0; i < guard; i++)
    {
        expect_int(guarded[i], 0xA5, "canary before");
        expect_int(guarded[guard + tiled + i], 0xA5, "canary after");
    }

    expect_texel(semi, width, 1, 1, 0x8001, "canary semi");
    expect_texel(semi, width, 0, 0, 0x0000, "canary semi other");
    expect_texel(primary, width, 1, 1, 0x0000, "canary primary semi");
    expect_texel(primary, width, 0, 0, 0x8001, "canary primary opaque");

    free(guarded);
    free(primary);
    free(src);
}

static void test_subimage(void)
{
    const int w = 16;
    const int h = 16;
    const int sw = 8;
    const int sh = 8;
    unsigned int tiled = GxRgb5a3TiledSize(w, h);
    unsigned char *src = malloc((size_t)sw * sh * 4);
    unsigned char *primary = calloc(tiled, 1);
    unsigned char *semi = calloc(tiled, 1);
    int textureType;

    /* Existing semi plane, partial update must not touch other texels. */
    fill_semi(semi, w, h, 0x800C); /* pre-existing semi outside the sub-rect */
    memset(src, 0, (size_t)sw * sh * 4);
    fill_opaque(src, sw, sh, 0x8001);
    set_texel(src, sw, 4, 4, 0x0004);
    textureType = GxScramble4b5a3Sub(src, primary, semi, 1, sw, sh, w);
    expect_flag(textureType, TEX_TYPE_1, 1, "sub type1");
    expect_texel(semi, w, 4, 4, 0x8004, "sub semi");
    expect_texel(semi, w, 12, 12, 0x800C, "sub outside preserved");

    /* Opaque-only partial update clears the old semi texel. */
    memset(src, 0, (size_t)sw * sh * 4);
    fill_opaque(src, sw, sh, 0x8001);
    textureType = GxScramble4b5a3Sub(src, primary, semi, 1, sw, sh, w);
    expect_flag(textureType, TEX_TYPE_1, 0, "sub opaque type1");
    expect_texel(semi, w, 4, 4, 0x0000, "sub semi cleared");
    expect_texel(semi, w, 12, 12, 0x800C, "sub outside after opaque");

    free(semi);
    free(primary);
    free(src);
}

static void test_first_partial(void)
{
    const int w = 16;
    const int h = 16;
    const int sw = 8;
    const int sh = 8;
    unsigned int tiled = GxRgb5a3TiledSize(w, h);
    unsigned char *src = malloc((size_t)sw * sh * 4);
    unsigned char *primary = calloc(tiled, 1);
    GxSemiPlaneState state = { NULL, 0 };
    GxSemiPlaneAction action;
    int reinit = 0;
    int textureType;

    s_allocCount = 0;

    memset(src, 0, (size_t)sw * sh * 4);
    fill_opaque(src, sw, sh, 0x8001);
    set_texel(src, sw, 4, 4, 0x0004);

    action = GxSemiPlanePrepareSub(&state, 1, &reinit);
    expect_int(action, GX_SEMI_ACTION_ALLOC_ZERO_COPY, "first partial action");
    expect_int(reinit, 1, "first partial reinit");

    /* First semi-touching partial upload allocates dirty memory and the
     * production-shared init helper must zero the whole plane before the
     * sub-rect write; nothing outside the sub-rect may survive. */
    state.data = alloc_tiled(w, h);
    memset(state.data, 0xA5, tiled); /* simulate reused/uninitialized memory */
    GxSemiPlaneZero(state.data, w, h);
    state.size = tiled;
    expect_int(s_allocCount, 1, "first partial alloc");

    textureType = GxScramble4b5a3Sub(src, primary, state.data, 1, sw, sh, w);
    expect_flag(textureType, TEX_TYPE_1, 1, "first partial type1");
    expect_texel(state.data, w, 4, 4, 0x8004, "first partial semi");
    expect_texel(state.data, w, 12, 12, 0x0000, "first partial outside zero");
    expect_texel(state.data, w, 15, 15, 0x0000, "first partial padding zero");

    free(state.data);
    free(primary);
    free(src);
}

int main(void)
{
    test_abcd();
    test_canary(256, 256);
    test_canary(512, 512);
    test_canary(2, 8);
    test_canary(5, 5);
    test_canary(7, 4);
    test_canary(4, 7);
    test_subimage();
    test_first_partial();

    if (s_failures == 0)
        printf("PASS semi_plane\n");
    else
        printf("FAIL semi_plane (%d)\n", s_failures);

    return s_failures == 0 ? 0 : 1;
}
