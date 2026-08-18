/***************************************************************************
                          gpuVramRect.h
                             -------------------
    Half-open VRAM rectangle helpers used by the GlesGpu texture cache
    invalidation paths.
 ***************************************************************************/

#ifndef GPU_VRAM_RECT_H
#define GPU_VRAM_RECT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct VramRect
{
    int x0;
    int y0;
    int x1;
    int y1;
} VramRect;

static inline int VramRectIsEmpty(const VramRect *r)
{
    return r == NULL || r->x0 >= r->x1 || r->y0 >= r->y1;
}

/*
 * Split one width/height VRAM rectangle into at most four non-wrapping,
 * half-open rectangles.  Right/bottom overflow wraps to the opposite edge;
 * negative origins are rejected because they must be clipped before wrap
 * handling.  Full-dimension coverage is normalized to one whole-frame piece.
 * Returns the number of pieces written (0 when the input is invalid/empty).
 */
static inline int SplitWrappedVramRect(int x, int y, int width, int height,
                                       int vram_width, int vram_height,
                                       VramRect out_rects[4])
{
    int count = 0;
    int x1, y1, wx, wy, mx1, my1;

    if (out_rects == NULL || width <= 0 || height <= 0 ||
        vram_width <= 0 || vram_height <= 0 ||
        x < 0 || y < 0 || x >= vram_width || y >= vram_height)
        return 0;

    if (width >= vram_width)
    {
        x = 0;
        width = vram_width;
    }
    if (height >= vram_height)
    {
        y = 0;
        height = vram_height;
    }

    x1 = x + width;
    y1 = y + height;
    wx = x1 > vram_width ? x1 - vram_width : 0;
    wy = y1 > vram_height ? y1 - vram_height : 0;

    if (wx == 0 && wy == 0)
    {
        out_rects[0].x0 = x;
        out_rects[0].y0 = y;
        out_rects[0].x1 = x1;
        out_rects[0].y1 = y1;
        return 1;
    }

    mx1 = wx > 0 ? vram_width : x1;
    my1 = wy > 0 ? vram_height : y1;

    if (x < mx1 && y < my1)
    {
        out_rects[count].x0 = x;
        out_rects[count].y0 = y;
        out_rects[count].x1 = mx1;
        out_rects[count].y1 = my1;
        count++;
    }

    if (wx > 0 && y < my1)
    {
        out_rects[count].x0 = 0;
        out_rects[count].y0 = y;
        out_rects[count].x1 = wx;
        out_rects[count].y1 = my1;
        count++;
    }

    if (wy > 0 && x < mx1)
    {
        out_rects[count].x0 = x;
        out_rects[count].y0 = 0;
        out_rects[count].x1 = mx1;
        out_rects[count].y1 = wy;
        count++;
    }

    if (wx > 0 && wy > 0)
    {
        out_rects[count].x0 = 0;
        out_rects[count].y0 = 0;
        out_rects[count].x1 = wx;
        out_rects[count].y1 = wy;
        count++;
    }

    return count;
}

typedef void (*VramRectCallback)(void *user_data, const VramRect *rect);

/*
 * Clip negative origins, split right/bottom VRAM wrap, and invoke callback
 * once per resulting half-open piece.  Returns the number of pieces.
 * This is the same entry point used by InvalidateTextureArea(), so host
 * tests can exercise the wrapper's clipping and piece dispatch directly.
 */
static inline int ForEachWrappedVramRect(int x, int y, int width, int height,
                                         int vram_width, int vram_height,
                                         VramRectCallback callback,
                                         void *user_data)
{
    VramRect pieces[4];
    int i, count;

    if (width <= 0 || height <= 0)
        return 0;

    if (x < 0)
    {
        width += x;
        x = 0;
    }
    if (y < 0)
    {
        height += y;
        y = 0;
    }
    if (width <= 0 || height <= 0)
        return 0;

    count = SplitWrappedVramRect(x, y, width, height,
                                 vram_width, vram_height, pieces);

    if (callback != NULL)
    {
        for (i = 0; i < count; i++)
            callback(user_data, &pieces[i]);
    }

    return count;
}

#ifdef __cplusplus
}
#endif

#endif /* GPU_VRAM_RECT_H */
