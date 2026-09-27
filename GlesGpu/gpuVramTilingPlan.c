#include "gpuVramTiling.h"

#include <string.h>

typedef struct GlesVramBoundsTag
{
    int minX;
    int minY;
    int maxX;
    int maxY;
    int valid;
} GlesVramBounds;

static uint32_t GlesVramReadLe32(const void *packet, int word)
{
    const unsigned char *p = (const unsigned char *)packet + word * 4;
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static int GlesVramIsPolylineEnd(uint32_t word)
{
    return (word & 0xf000f000u) == 0x50005000u;
}

static void GlesVramBoundsAdd(GlesVramBounds *bounds, uint32_t word,
                              const GlesVramPoint *drawOffset)
{
    int x = (int)(int16_t)(word & 0xffffu) + drawOffset->x;
    int y = (int)(int16_t)(word >> 16) + drawOffset->y;

    if (!bounds->valid)
    {
        bounds->minX = bounds->maxX = x;
        bounds->minY = bounds->maxY = y;
        bounds->valid = 1;
        return;
    }
    if (x < bounds->minX) bounds->minX = x;
    if (x > bounds->maxX) bounds->maxX = x;
    if (y < bounds->minY) bounds->minY = y;
    if (y > bounds->maxY) bounds->maxY = y;
}

static int GlesVramClipPrimitiveBounds(const GlesVramBounds *bounds,
                                       const GlesVramRect *drawArea,
                                       GlesVramRect *rect)
{
    int x0;
    int y0;
    int x1;
    int y1;

    if (!bounds->valid || drawArea == NULL ||
        drawArea->width <= 0 || drawArea->height <= 0)
        return 0;

    x0 = bounds->minX;
    y0 = bounds->minY;
    x1 = bounds->maxX + 1;
    y1 = bounds->maxY + 1;
    if (x0 < drawArea->x) x0 = drawArea->x;
    if (y0 < drawArea->y) y0 = drawArea->y;
    if (x1 > drawArea->x + drawArea->width)
        x1 = drawArea->x + drawArea->width;
    if (y1 > drawArea->y + drawArea->height)
        y1 = drawArea->y + drawArea->height;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > GLES_VRAM_WIDTH) x1 = GLES_VRAM_WIDTH;
    if (y1 > GLES_VRAM_HEIGHT) y1 = GLES_VRAM_HEIGHT;
    if (x1 <= x0 || y1 <= y0)
        return 0;

    rect->x = x0;
    rect->y = y0;
    rect->width = x1 - x0;
    rect->height = y1 - y0;
    return 1;
}

static int GlesVramBuildFillPlan(const void *packet, int packetWords,
                                 GlesVramPrimitivePlan *plan)
{
    uint32_t position;
    uint32_t size;
    int width;
    int height;

    if (packetWords < 3)
        return 0;

    position = GlesVramReadLe32(packet, 1);
    size = GlesVramReadLe32(packet, 2);
    width = (int)(size & 0x3ffu);
    height = (int)((size >> 16) & 0x1ffu);
    if (width <= 0 || height <= 0)
        return 0;

    plan->writeRect.x = (int)(position & 0x3f0u);
    plan->writeRect.y = (int)((position >> 16) & 0x1ffu);
    plan->writeRect.width = (width + 15) & ~15;
    plan->writeRect.height = height;
    if (plan->writeRect.width > GLES_VRAM_WIDTH)
        plan->writeRect.width = GLES_VRAM_WIDTH;
    plan->spanCount = GlesVramBuildTileSpans(&plan->writeRect, plan->span);
    plan->isFill = 1;
    return plan->spanCount > 0;
}

static int GlesVramBuildRectPrimitivePlan(unsigned char command,
                                          const void *packet,
                                          int packetWords,
                                          const GlesVramRect *drawArea,
                                          const GlesVramPoint *drawOffset,
                                          GlesVramPrimitivePlan *plan)
{
    uint32_t position;
    uint32_t size = 0;
    GlesVramBounds bounds = {0};
    int width;
    int height;
    int sizeWord;

    if (packetWords < 2)
        return 0;
    position = GlesVramReadLe32(packet, 1);

    if (command < 0x68)
    {
        sizeWord = (command & 4) ? 3 : 2;
        if (packetWords <= sizeWord)
            return 0;
        size = GlesVramReadLe32(packet, sizeWord);
        width = (int)(size & 0xffffu);
        height = (int)(size >> 16);
    }
    else if (command < 0x70)
        width = height = 1;
    else if (command < 0x78)
        width = height = 8;
    else
        width = height = 16;

    if (width <= 0 || height <= 0)
        return 0;

    GlesVramBoundsAdd(&bounds, position, drawOffset);
    bounds.maxX = bounds.minX + width - 1;
    bounds.maxY = bounds.minY + height - 1;
    if (!GlesVramClipPrimitiveBounds(&bounds, drawArea, &plan->writeRect))
        return 0;
    plan->spanCount = GlesVramBuildTileSpans(&plan->writeRect, plan->span);
    return plan->spanCount > 0;
}

static int GlesVramBuildVertexPrimitivePlan(unsigned char command,
                                            const void *packet,
                                            int packetWords,
                                            const GlesVramRect *drawArea,
                                            const GlesVramPoint *drawOffset,
                                            GlesVramPrimitivePlan *plan)
{
    static const unsigned char triF[] = {1, 2, 3};
    static const unsigned char triFT[] = {1, 3, 5};
    static const unsigned char quadF[] = {1, 2, 3, 4};
    static const unsigned char quadFT[] = {1, 3, 5, 7};
    static const unsigned char triG[] = {1, 3, 5};
    static const unsigned char triGT[] = {1, 4, 7};
    static const unsigned char quadG[] = {1, 3, 5, 7};
    static const unsigned char quadGT[] = {1, 4, 7, 10};
    static const unsigned char lineF[] = {1, 2};
    static const unsigned char lineG[] = {1, 3};
    const unsigned char *indices = NULL;
    int indexCount = 0;
    int i;
    int step = 0;
    int isLine = 0;
    GlesVramBounds bounds = {0};

    if (command >= 0x20 && command <= 0x23)
        indices = triF, indexCount = 3;
    else if (command <= 0x27)
        indices = triFT, indexCount = 3;
    else if (command <= 0x2b)
        indices = quadF, indexCount = 4;
    else if (command <= 0x2f)
        indices = quadFT, indexCount = 4;
    else if (command <= 0x33)
        indices = triG, indexCount = 3;
    else if (command <= 0x37)
        indices = triGT, indexCount = 3;
    else if (command <= 0x3b)
        indices = quadG, indexCount = 4;
    else if (command <= 0x3f)
        indices = quadGT, indexCount = 4;
    else if (command <= 0x47)
        indices = lineF, indexCount = 2, isLine = 1;
    else if (command >= 0x50 && command <= 0x57)
        indices = lineG, indexCount = 2, isLine = 1;
    else if (command >= 0x48 && command <= 0x4f)
        i = 1, step = 1, isLine = 1;
    else if (command >= 0x58 && command <= 0x5f)
        i = 1, step = 2, isLine = 1;
    else
        return 0;

    if (indices != NULL)
    {
        for (i = 0; i < indexCount; i++)
        {
            if (indices[i] >= packetWords)
                return 0;
            GlesVramBoundsAdd(&bounds,
                GlesVramReadLe32(packet, indices[i]), drawOffset);
        }
    }
    else
    {
        int vertices = 0;
        for (; i < packetWords; i += step)
        {
            uint32_t word = GlesVramReadLe32(packet, i);
            if (GlesVramIsPolylineEnd(word))
                break;
            GlesVramBoundsAdd(&bounds, word, drawOffset);
            vertices++;
            if (step == 2 && i + 1 < packetWords &&
                GlesVramIsPolylineEnd(GlesVramReadLe32(packet, i + 1)))
                break;
        }
        if (vertices < 2)
            return 0;
    }

    /* offsetline() turns a PS line into a narrow four-vertex strip.  Its
     * +1 endpoint convention can reach one pixel beyond the greatest source
     * X/Y.  Include that actual GLES footprint before applying DrawArea and
     * tile scissors, otherwise a line ending at X=639 loses its RIGHT pass. */
    if (isLine)
    {
        bounds.maxX++;
        bounds.maxY++;
    }

    if (!GlesVramClipPrimitiveBounds(&bounds, drawArea, &plan->writeRect))
        return 0;
    plan->spanCount = GlesVramBuildTileSpans(&plan->writeRect, plan->span);
    return plan->spanCount > 0;
}

int GlesVramBuildPrimitivePlan(unsigned char command,
                               const void *packet, int packetWords,
                               const GlesVramRect *drawArea,
                               const GlesVramPoint *drawOffset,
                               GlesVramPrimitivePlan *plan)
{
    if (packet == NULL || plan == NULL || packetWords <= 0)
        return 0;

    memset(plan, 0, sizeof(*plan));
    if (command == 0x02)
        return GlesVramBuildFillPlan(packet, packetWords, plan);

    if (drawArea == NULL || drawOffset == NULL ||
        command < 0x20 || command > 0x7f)
        return 0;

    if (command >= 0x60)
        return GlesVramBuildRectPrimitivePlan(command, packet, packetWords,
                                              drawArea, drawOffset, plan);
    return GlesVramBuildVertexPrimitivePlan(command, packet, packetWords,
                                            drawArea, drawOffset, plan);
}
