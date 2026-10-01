#include "gpuDrawFootprint.h"

#include <stddef.h>
#include <string.h>

typedef struct GlesGpuDrawPointTag
{
    int x;
    int y;
} GlesGpuDrawPoint;

static uint32_t GlesGpuDrawReadLittle32(const unsigned char *packet,
                                        int index)
{
    const unsigned char *word = packet + index * 4;
    return (uint32_t)word[0] |
           ((uint32_t)word[1] << 8) |
           ((uint32_t)word[2] << 16) |
           ((uint32_t)word[3] << 24);
}

static int GlesGpuDrawSignExtend11(unsigned int value)
{
    value &= 0x7ffu;
    return (value & 0x400u) != 0 ? (int)value - 0x800 : (int)value;
}

static GlesGpuDrawPoint GlesGpuDrawDecodePoint(uint32_t word,
                                               int offsetX, int offsetY,
                                               int truncateAfterOffset)
{
    GlesGpuDrawPoint point;

    point.x = GlesGpuDrawSignExtend11(word) + offsetX;
    point.y = GlesGpuDrawSignExtend11(word >> 16) + offsetY;
    if (truncateAfterOffset)
    {
        point.x = GlesGpuDrawSignExtend11((unsigned int)point.x);
        point.y = GlesGpuDrawSignExtend11((unsigned int)point.y);
    }
    return point;
}

static int GlesGpuDrawRectEqual(const GlesVramCommandRect *first,
                                const GlesVramCommandRect *second)
{
    return first->x == second->x && first->y == second->y &&
           first->width == second->width &&
           first->height == second->height;
}

static void GlesGpuDrawUnionRect(GlesVramCommandRect *destination,
                                 int *destinationValid,
                                 const GlesVramCommandRect *source)
{
    int x0;
    int y0;
    int x1;
    int y1;

    if (source->width <= 0 || source->height <= 0)
        return;
    if (!*destinationValid)
    {
        *destination = *source;
        *destinationValid = 1;
        return;
    }

    x0 = destination->x < source->x ? destination->x : source->x;
    y0 = destination->y < source->y ? destination->y : source->y;
    x1 = destination->x + destination->width > source->x + source->width ?
         destination->x + destination->width : source->x + source->width;
    y1 = destination->y + destination->height > source->y + source->height ?
         destination->y + destination->height : source->y + source->height;
    destination->x = x0;
    destination->y = y0;
    destination->width = x1 - x0;
    destination->height = y1 - y0;
}

static void GlesGpuDrawAddTriangle(const GlesGpuDrawPoint *first,
                                   const GlesGpuDrawPoint *second,
                                   const GlesGpuDrawPoint *third,
                                   GlesVramCommandRect *combined,
                                   int *combinedValid)
{
    GlesVramCommandRect rect;
    int maximumX;
    int maximumY;

    rect.x = first->x;
    if (second->x < rect.x) rect.x = second->x;
    if (third->x < rect.x) rect.x = third->x;
    rect.y = first->y;
    if (second->y < rect.y) rect.y = second->y;
    if (third->y < rect.y) rect.y = third->y;
    maximumX = first->x;
    if (second->x > maximumX) maximumX = second->x;
    if (third->x > maximumX) maximumX = third->x;
    maximumY = first->y;
    if (second->y > maximumY) maximumY = second->y;
    if (third->y > maximumY) maximumY = third->y;
    rect.width = maximumX - rect.x + 1;
    rect.height = maximumY - rect.y + 1;

    /* The PS1 rejects primitives spanning more than the VRAM dimensions. */
    if (rect.width > GLES_VRAM_COMMAND_WIDTH ||
        rect.height > GLES_VRAM_COMMAND_HEIGHT)
        return;
    GlesGpuDrawUnionRect(combined, combinedValid, &rect);
}

static void GlesGpuDrawAddLine(const GlesGpuDrawPoint *first,
                               const GlesGpuDrawPoint *second,
                               GlesVramCommandRect *combined,
                               int *combinedValid)
{
    GlesVramCommandRect rect;
    int maximumX = first->x > second->x ? first->x : second->x;
    int maximumY = first->y > second->y ? first->y : second->y;

    rect.x = first->x < second->x ? first->x : second->x;
    rect.y = first->y < second->y ? first->y : second->y;
    if (maximumX - rect.x + 1 > GLES_VRAM_COMMAND_WIDTH ||
        maximumY - rect.y + 1 > GLES_VRAM_COMMAND_HEIGHT)
        return;

    /* offsetline() expands the PS1 line into a narrow GX quad.  Its +1
     * origin and half-pixel construction can touch one extra pixel at the
     * maximum edge, so keep that pixel in the dependency footprint. */
    rect.width = maximumX - rect.x + 2;
    rect.height = maximumY - rect.y + 2;
    GlesGpuDrawUnionRect(combined, combinedValid, &rect);
}

static int GlesGpuDrawClip(const GlesVramCommandRect *source,
                           const GlesGpuDrawFootprintState *state,
                           GlesVramCommandRect *clipped)
{
    GlesVramCommandRect vram;
    GlesVramCommandRect drawing;

    vram.x = 0;
    vram.y = 0;
    vram.width = GLES_VRAM_COMMAND_WIDTH;
    vram.height = GLES_VRAM_COMMAND_HEIGHT;
    if (!GlesVramCommandIntersectRects(source, &vram, &drawing))
        return 0;
    return GlesVramCommandIntersectRects(&drawing, &state->drawArea,
                                         clipped);
}

static int GlesGpuDrawPlanPolygon(const unsigned char *packet,
                                  int wordCount, unsigned int opcode,
                                  const GlesGpuDrawFootprintState *state,
                                  GlesVramCommandRect *rect)
{
    GlesGpuDrawPoint point[4];
    int textured = (opcode & 0x04u) != 0;
    int quad = (opcode & 0x08u) != 0;
    int gouraud = (opcode & 0x10u) != 0;
    int stride = 1 + textured + gouraud;
    int vertices = quad ? 4 : 3;
    int valid = 0;
    int vertex;

    for (vertex = 0; vertex < vertices; vertex++)
    {
        int index = 1 + vertex * stride;
        if (index >= wordCount)
            return -1;
        point[vertex] = GlesGpuDrawDecodePoint(
            GlesGpuDrawReadLittle32(packet, index),
            state->drawOffsetX, state->drawOffsetY, 0);
    }

    GlesGpuDrawAddTriangle(&point[0], &point[1], &point[2], rect, &valid);
    if (quad)
        GlesGpuDrawAddTriangle(&point[1], &point[2], &point[3], rect, &valid);
    return valid;
}

static int GlesGpuDrawPlanLine(const unsigned char *packet,
                               int wordCount, unsigned int opcode,
                               const GlesGpuDrawFootprintState *state,
                               GlesVramCommandRect *rect)
{
    int gouraud = (opcode & 0x10u) != 0;
    int polyline = (opcode & 0x08u) != 0;
    int valid = 0;
    GlesGpuDrawPoint first;
    GlesGpuDrawPoint second;

    if (!polyline)
    {
        int secondIndex = gouraud ? 3 : 2;
        if (wordCount <= secondIndex)
            return -1;
        first = GlesGpuDrawDecodePoint(
            GlesGpuDrawReadLittle32(packet, 1),
            state->drawOffsetX, state->drawOffsetY, 0);
        second = GlesGpuDrawDecodePoint(
            GlesGpuDrawReadLittle32(packet, secondIndex),
            state->drawOffsetX, state->drawOffsetY, 0);
        GlesGpuDrawAddLine(&first, &second, rect, &valid);
        return valid;
    }

    if (wordCount < (gouraud ? 5 : 4))
        return -1;
    first = GlesGpuDrawDecodePoint(
        GlesGpuDrawReadLittle32(packet, 1),
        state->drawOffsetX, state->drawOffsetY, 0);
    if (gouraud)
    {
        int index;
        for (index = 3; index < wordCount; index += 2)
        {
            uint32_t word;
            if (index + 1 >= wordCount)
                return -1;
            word = GlesGpuDrawReadLittle32(packet, index);
            second = GlesGpuDrawDecodePoint(
                word, state->drawOffsetX, state->drawOffsetY, 0);
            GlesGpuDrawAddLine(&first, &second, rect, &valid);
            first = second;
            if ((GlesGpuDrawReadLittle32(packet, index + 1) &
                 0xf000f000u) == 0x50005000u)
                return valid;
        }
    }
    else
    {
        int index;
        for (index = 2; index < wordCount; index++)
        {
            uint32_t word = GlesGpuDrawReadLittle32(packet, index);
            if ((word & 0xf000f000u) == 0x50005000u)
                return index >= 3 ? valid : -1;
            second = GlesGpuDrawDecodePoint(
                word, state->drawOffsetX, state->drawOffsetY, 0);
            GlesGpuDrawAddLine(&first, &second, rect, &valid);
            first = second;
        }
    }
    return -1;
}

static int GlesGpuDrawPlanRectangle(const unsigned char *packet,
                                    int wordCount, unsigned int opcode,
                                    const GlesGpuDrawFootprintState *state,
                                    GlesVramCommandRect *rect)
{
    GlesGpuDrawPoint point;
    unsigned int sizeCode = (opcode >> 3) & 0x03u;
    int textured = (opcode & 0x04u) != 0;

    if (wordCount < 2)
        return -1;
    point = GlesGpuDrawDecodePoint(GlesGpuDrawReadLittle32(packet, 1),
                                   state->drawOffsetX,
                                   state->drawOffsetY, 1);
    rect->x = point.x;
    rect->y = point.y;
    if (sizeCode == 0)
    {
        int sizeIndex = textured ? 3 : 2;
        uint32_t size;
        if (wordCount <= sizeIndex)
            return -1;
        size = GlesGpuDrawReadLittle32(packet, sizeIndex);
        rect->width = (int)(size & 0x3ffu);
        rect->height = (int)((size >> 16) & 0x1ffu);
    }
    else if (sizeCode == 1)
    {
        rect->width = 1;
        rect->height = 1;
    }
    else if (sizeCode == 2)
    {
        rect->width = 8;
        rect->height = 8;
    }
    else
    {
        rect->width = 16;
        rect->height = 16;
    }
    return rect->width > 0 && rect->height > 0;
}

int GlesGpuPlanDrawFootprint(const void *packet, int wordCount,
                             const GlesGpuDrawFootprintState *state,
                             GlesGpuDrawFootprint *footprint)
{
    const unsigned char *bytes = (const unsigned char *)packet;
    GlesVramCommandRect raw;
    unsigned int opcode;
    int planned;

    if (footprint != NULL)
        memset(footprint, 0, sizeof(*footprint));
    if (packet == NULL || state == NULL || footprint == NULL ||
        wordCount <= 0 || state->drawArea.width <= 0 ||
        state->drawArea.height <= 0)
        return 0;

    opcode = GlesGpuDrawReadLittle32(bytes, 0) >> 24;
    if (opcode >= 0x20u && opcode <= 0x3fu)
    {
        footprint->kind = GLES_GPU_DRAW_FOOTPRINT_POLYGON;
        planned = GlesGpuDrawPlanPolygon(bytes, wordCount, opcode,
                                         state, &raw);
    }
    else if (opcode >= 0x40u && opcode <= 0x5fu &&
             (opcode & 0x04u) == 0)
    {
        footprint->kind = GLES_GPU_DRAW_FOOTPRINT_LINE;
        planned = GlesGpuDrawPlanLine(bytes, wordCount, opcode,
                                      state, &raw);
    }
    else if (opcode >= 0x60u && opcode <= 0x7fu)
    {
        footprint->kind = GLES_GPU_DRAW_FOOTPRINT_RECTANGLE;
        planned = GlesGpuDrawPlanRectangle(bytes, wordCount, opcode,
                                           state, &raw);
    }
    else
        return 0;

    if (planned < 0)
    {
        footprint->malformed = 1;
        return 1;
    }
    if (!planned)
    {
        footprint->discarded = 1;
        return 1;
    }

    footprint->unclippedRect = raw;
    if (!GlesGpuDrawClip(&raw, state, &footprint->rect))
    {
        footprint->whollyOutside = 1;
        return 1;
    }
    footprint->hasPixels = 1;
    footprint->clipped = !GlesGpuDrawRectEqual(&raw, &footprint->rect);
    return 1;
}
