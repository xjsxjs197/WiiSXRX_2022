#include "gpuVramTiling.h"

static int GlesVramNormalizeCoordinate(int value, int limit)
{
    int normalized = value % limit;
    return normalized < 0 ? normalized + limit : normalized;
}

void GlesVramDecodeTransferSize(uint32_t sizeWord,
                                int *width, int *height)
{
    int decodedWidth = (int)(sizeWord & 0x3ffu);
    int decodedHeight = (int)((sizeWord >> 16) & 0x1ffu);

    if (width != NULL)
        *width = decodedWidth != 0 ? decodedWidth : GLES_VRAM_WIDTH;
    if (height != NULL)
        *height = decodedHeight != 0 ? decodedHeight : GLES_VRAM_HEIGHT;
}

int GlesVramSplitWrappedRect(const GlesVramRect *input,
                             GlesVramRectPiece output[
                                 GLES_VRAM_MAX_WRAP_PIECES])
{
    int xLength[2];
    int yLength[2];
    int xOffset[2];
    int yOffset[2];
    int xStart[2];
    int yStart[2];
    int xCount = 1;
    int yCount = 1;
    int x;
    int y;
    int ix;
    int iy;
    int count = 0;

    if (input == NULL || output == NULL ||
        input->width <= 0 || input->height <= 0 ||
        input->width > GLES_VRAM_WIDTH ||
        input->height > GLES_VRAM_HEIGHT)
        return 0;

    x = GlesVramNormalizeCoordinate(input->x, GLES_VRAM_WIDTH);
    y = GlesVramNormalizeCoordinate(input->y, GLES_VRAM_HEIGHT);

    xStart[0] = x;
    xOffset[0] = 0;
    xLength[0] = input->width;
    if (x + input->width > GLES_VRAM_WIDTH)
    {
        xLength[0] = GLES_VRAM_WIDTH - x;
        xStart[1] = 0;
        xOffset[1] = xLength[0];
        xLength[1] = input->width - xLength[0];
        xCount = 2;
    }

    yStart[0] = y;
    yOffset[0] = 0;
    yLength[0] = input->height;
    if (y + input->height > GLES_VRAM_HEIGHT)
    {
        yLength[0] = GLES_VRAM_HEIGHT - y;
        yStart[1] = 0;
        yOffset[1] = yLength[0];
        yLength[1] = input->height - yLength[0];
        yCount = 2;
    }

    for (iy = 0; iy < yCount; iy++)
    {
        for (ix = 0; ix < xCount; ix++)
        {
            GlesVramRectPiece *piece = &output[count++];
            piece->rect.x = xStart[ix];
            piece->rect.y = yStart[iy];
            piece->rect.width = xLength[ix];
            piece->rect.height = yLength[iy];
            piece->commandX = xOffset[ix];
            piece->commandY = yOffset[iy];
        }
    }

    return count;
}

static void GlesVramAddTileSpan(const GlesVramRectPiece *piece,
                                int x0, int x1,
                                GlesVramTileId tile,
                                GlesVramTileSpan *span)
{
    int originX = tile == GLES_VRAM_TILE_RIGHT ? GLES_VRAM_RIGHT_X : 0;

    span->tile = tile;
    span->vramRect.x = x0;
    span->vramRect.y = piece->rect.y;
    span->vramRect.width = x1 - x0;
    span->vramRect.height = piece->rect.height;
    span->localRect.x = x0 - originX;
    span->localRect.y = piece->rect.y;
    span->localRect.width = x1 - x0;
    span->localRect.height = piece->rect.height;
    span->commandX = piece->commandX + (x0 - piece->rect.x);
    span->commandY = piece->commandY;
}

int GlesVramBuildTileSpans(const GlesVramRect *input,
                           GlesVramTileSpan output[
                               GLES_VRAM_MAX_TILE_SPANS])
{
    GlesVramRectPiece pieces[GLES_VRAM_MAX_WRAP_PIECES];
    int pieceCount;
    int pieceIndex;
    int count = 0;

    if (output == NULL)
        return 0;

    pieceCount = GlesVramSplitWrappedRect(input, pieces);
    for (pieceIndex = 0; pieceIndex < pieceCount; pieceIndex++)
    {
        const GlesVramRectPiece *piece = &pieces[pieceIndex];
        int x0 = piece->rect.x;
        int x1 = x0 + piece->rect.width;

        if (x0 < GLES_VRAM_LEFT_WIDTH)
        {
            int leftEnd = x1 < GLES_VRAM_LEFT_WIDTH ?
                          x1 : GLES_VRAM_LEFT_WIDTH;
            GlesVramAddTileSpan(piece, x0, leftEnd,
                                GLES_VRAM_TILE_LEFT, &output[count++]);
        }

        if (x1 > GLES_VRAM_RIGHT_X)
        {
            int rightStart = x0 > GLES_VRAM_RIGHT_X ?
                             x0 : GLES_VRAM_RIGHT_X;
            GlesVramAddTileSpan(piece, rightStart, x1,
                                GLES_VRAM_TILE_RIGHT, &output[count++]);
        }
    }

    return count;
}

int GlesVramBuildDisplayPlan(const GlesVramRect *displayRect,
                             GlesVramDisplayPlan *plan)
{
    GlesVramTileSpan spans[GLES_VRAM_MAX_TILE_SPANS];
    int count;
    int index;

    if (displayRect == NULL || plan == NULL)
        return 0;
    count = GlesVramBuildTileSpans(displayRect, spans);
    if (count <= 0 || count > GLES_VRAM_MAX_DISPLAY_SEGMENTS)
        return 0;

    plan->displayRect = *displayRect;
    plan->segmentCount = count;
    for (index = 0; index < count; index++)
    {
        GlesVramDisplaySegment *segment = &plan->segment[index];
        segment->tile = spans[index].tile;
        segment->sourceRect = spans[index].localRect;
        segment->outputRect.x = spans[index].commandX;
        segment->outputRect.y = spans[index].commandY;
        segment->outputRect.width = spans[index].localRect.width;
        segment->outputRect.height = spans[index].localRect.height;
    }
    return 1;
}
