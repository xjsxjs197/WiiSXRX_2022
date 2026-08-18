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

static inline int VramRectIntersects(const VramRect *a, const VramRect *b)
{
    return a != NULL && b != NULL &&
           a->x0 < b->x1 && b->x0 < a->x1 &&
           a->y0 < b->y1 && b->y0 < a->y1;
}

/*
 * Texture-window source footprint in PS1 VRAM word coordinates.
 * pageid encodes the 64-word x-page and the 256-row y-page; textureMode
 * selects the footprint width (64/128/256 words for 4/8/15-bit).
 * Returns the number of non-wrapping half-open pieces written to out_rects.
 */
static inline int TextureWindowSourceRects(int pageid, int textureMode,
                                           int vram_width, int vram_height,
                                           VramRect out_rects[4])
{
    int xpage, ypage, width;

    if (out_rects == NULL || pageid < 0 || textureMode < 0 || textureMode > 2 ||
        vram_width <= 0 || vram_height <= 0)
        return 0;

    xpage = pageid & 15;
    ypage = pageid >> 4;
    width = 64 << textureMode;

    return SplitWrappedVramRect(xpage << 6, ypage << 8, width, 256,
                                vram_width, vram_height, out_rects);
}

/*
 * Linear-read source footprint used by the current window loaders.  When the
 * page footprint crosses X=1024, the loaders advance their pointers into the
 * next VRAM row instead of wrapping within the same row.  T2 conservatively
 * tracks both the hardware row-local ranges and these linear ranges until the
 * loaders are corrected to row-wrap.
 */
static inline int TextureWindowSourceLinearRects(int pageid, int textureMode,
                                                 int vram_width, int vram_height,
                                                 VramRect out_rects[4])
{
    int xpage, ypage, width, x0, y0, x1, overflow;

    if (out_rects == NULL || pageid < 0 || textureMode < 0 || textureMode > 2 ||
        vram_width <= 0 || vram_height <= 0)
        return 0;

    xpage = pageid & 15;
    ypage = pageid >> 4;
    width = 64 << textureMode;
    x0 = xpage << 6;
    y0 = ypage << 8;
    x1 = x0 + width;

    if (x1 <= vram_width)
    {
        out_rects[0].x0 = x0;
        out_rects[0].y0 = y0;
        out_rects[0].x1 = x1;
        out_rects[0].y1 = y0 + 256;
        if (out_rects[0].y1 > vram_height)
            out_rects[0].y1 = vram_height;
        return VramRectIsEmpty(&out_rects[0]) ? 0 : 1;
    }

    overflow = x1 - vram_width;

    out_rects[0].x0 = x0;
    out_rects[0].y0 = y0;
    out_rects[0].x1 = vram_width;
    out_rects[0].y1 = y0 + 256;
    if (out_rects[0].y1 > vram_height)
        out_rects[0].y1 = vram_height;

    out_rects[1].x0 = 0;
    out_rects[1].y0 = y0 + 1;
    out_rects[1].x1 = overflow;
    out_rects[1].y1 = y0 + 257;
    if (out_rects[1].y1 > vram_height)
        out_rects[1].y1 = vram_height;

    return (VramRectIsEmpty(&out_rects[0]) ? 0 : 1) +
           (VramRectIsEmpty(&out_rects[1]) ? 0 : 1);
}

/*
 * Texture-window palette footprint in PS1 VRAM word coordinates.
 * mode 0 uses 16 words, mode 1 uses 256 words.  Returns the number of
 * wrap-split half-open rects written to out_rects.
 */
static inline int TextureWindowPaletteRects(unsigned int clutId, int textureMode,
                                            int clut_y_mask,
                                            int vram_width, int vram_height,
                                            VramRect out_rects[4])
{
    int cx, cy, width;

    if (out_rects == NULL || textureMode < 0 || textureMode > 1 ||
        vram_width <= 0 || vram_height <= 0 || clut_y_mask < 0)
        return 0;

    cx = ((int)(clutId & 0x3F) << 4);
    cy = (int)((clutId >> 6) & clut_y_mask);
    width = textureMode == 0 ? 16 : 256;

    return SplitWrappedVramRect(cx, cy, width, 1,
                                vram_width, vram_height, out_rects);
}

/*
 * Linear-read palette footprint used by the current palette loader/checksum.
 * Crossing X=1024 advances into the next VRAM row.  T2 tracks this range in
 * addition to the hardware row-local range until the loaders are corrected.
 */
static inline int TextureWindowPaletteLinearRects(unsigned int clutId,
                                                  int textureMode,
                                                  int clut_y_mask,
                                                  int vram_width, int vram_height,
                                                  VramRect out_rects[4])
{
    int cx, cy, width, x1, overflow;

    if (out_rects == NULL || textureMode < 0 || textureMode > 1 ||
        vram_width <= 0 || vram_height <= 0 || clut_y_mask < 0)
        return 0;

    cx = ((int)(clutId & 0x3F) << 4);
    cy = (int)((clutId >> 6) & clut_y_mask);
    width = textureMode == 0 ? 16 : 256;
    x1 = cx + width;

    if (x1 <= vram_width)
    {
        out_rects[0].x0 = cx;
        out_rects[0].y0 = cy;
        out_rects[0].x1 = x1;
        out_rects[0].y1 = cy + 1;
        return VramRectIsEmpty(&out_rects[0]) ? 0 : 1;
    }

    overflow = x1 - vram_width;

    out_rects[0].x0 = cx;
    out_rects[0].y0 = cy;
    out_rects[0].x1 = vram_width;
    out_rects[0].y1 = cy + 1;

    out_rects[1].x0 = 0;
    out_rects[1].y0 = cy + 1;
    out_rects[1].x1 = overflow;
    out_rects[1].y1 = cy + 2;
    if (out_rects[1].y1 > vram_height)
        out_rects[1].y1 = vram_height;

    return (VramRectIsEmpty(&out_rects[0]) ? 0 : 1) +
           (VramRectIsEmpty(&out_rects[1]) ? 0 : 1);
}

/*
 * Pure cache-entry dependency decision used by InvalidateWndTextureArea().
 * storedClutId is the entry's ClutID after checksum/semi-transparency bits
 * were added; clut_mask extracts the original CLUT bits.  Both the hardware
 * row-local ranges and the current linear loader ranges are checked.
 */
static inline int TextureWindowEntryDependsOnRect(unsigned int storedClutId,
                                                  int pageid, int textureMode,
                                                  unsigned int clut_mask,
                                                  int clut_y_mask,
                                                  int vram_width, int vram_height,
                                                  const VramRect *rect)
{
    VramRect pieces[4];
    int i, n;

    if (rect == NULL || VramRectIsEmpty(rect))
        return 0;

    n = TextureWindowSourceRects(pageid, textureMode,
                                 vram_width, vram_height, pieces);
    for (i = 0; i < n; i++)
        if (VramRectIntersects(rect, &pieces[i]))
            return 1;

    n = TextureWindowSourceLinearRects(pageid, textureMode,
                                       vram_width, vram_height, pieces);
    for (i = 0; i < n; i++)
        if (VramRectIntersects(rect, &pieces[i]))
            return 1;

    if (textureMode != 2)
    {
        n = TextureWindowPaletteRects(storedClutId & clut_mask, textureMode,
                                      clut_y_mask, vram_width, vram_height,
                                      pieces);
        for (i = 0; i < n; i++)
            if (VramRectIntersects(rect, &pieces[i]))
                return 1;

        n = TextureWindowPaletteLinearRects(storedClutId & clut_mask,
                                            textureMode, clut_y_mask,
                                            vram_width, vram_height, pieces);
        for (i = 0; i < n; i++)
            if (VramRectIntersects(rect, &pieces[i]))
                return 1;
    }

    return 0;
}

#ifdef __cplusplus
}
#endif

#endif /* GPU_VRAM_RECT_H */
