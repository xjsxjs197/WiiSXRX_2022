#include "gpuVramCommandRect.h"

#include <stddef.h>

static int GlesVramCommandNormalizeCoordinate(int value, int limit)
{
    int normalized = value % limit;
    return normalized < 0 ? normalized + limit : normalized;
}

void GlesVramCommandDecodeTransferSize(uint32_t sizeWord,
                                       int *width, int *height)
{
    int decodedWidth = (int)(sizeWord & 0x3ffu);
    int decodedHeight = (int)((sizeWord >> 16) & 0x1ffu);

    if (width != NULL)
        *width = decodedWidth != 0 ? decodedWidth :
                 GLES_VRAM_COMMAND_WIDTH;
    if (height != NULL)
        *height = decodedHeight != 0 ? decodedHeight :
                  GLES_VRAM_COMMAND_HEIGHT;
}

int GlesVramCommandDecodeFill(uint32_t positionWord, uint32_t sizeWord,
                              GlesVramCommandRect *rect)
{
    int width;
    int height;

    if (rect == NULL)
        return 0;

    width = (int)(sizeWord & 0x3ffu);
    height = (int)((sizeWord >> 16) & 0x1ffu);
    if (width == 0 || height == 0)
        return 0;

    rect->x = (int)(positionWord & 0x3f0u);
    rect->y = (int)((positionWord >> 16) & 0x1ffu);
    rect->width = (width + 15) & ~15;
    rect->height = height;
    if (rect->width > GLES_VRAM_COMMAND_WIDTH)
        rect->width = GLES_VRAM_COMMAND_WIDTH;
    return 1;
}

int GlesVramCommandSplitWrappedRect(
    const GlesVramCommandRect *input,
    GlesVramCommandRectPiece output[
        GLES_VRAM_COMMAND_MAX_WRAP_PIECES])
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
        input->width > GLES_VRAM_COMMAND_WIDTH ||
        input->height > GLES_VRAM_COMMAND_HEIGHT)
        return 0;

    x = GlesVramCommandNormalizeCoordinate(
        input->x, GLES_VRAM_COMMAND_WIDTH);
    y = GlesVramCommandNormalizeCoordinate(
        input->y, GLES_VRAM_COMMAND_HEIGHT);

    xStart[0] = x;
    xOffset[0] = 0;
    xLength[0] = input->width;
    if (x + input->width > GLES_VRAM_COMMAND_WIDTH)
    {
        xLength[0] = GLES_VRAM_COMMAND_WIDTH - x;
        xStart[1] = 0;
        xOffset[1] = xLength[0];
        xLength[1] = input->width - xLength[0];
        xCount = 2;
    }

    yStart[0] = y;
    yOffset[0] = 0;
    yLength[0] = input->height;
    if (y + input->height > GLES_VRAM_COMMAND_HEIGHT)
    {
        yLength[0] = GLES_VRAM_COMMAND_HEIGHT - y;
        yStart[1] = 0;
        yOffset[1] = yLength[0];
        yLength[1] = input->height - yLength[0];
        yCount = 2;
    }

    for (iy = 0; iy < yCount; iy++)
    {
        for (ix = 0; ix < xCount; ix++)
        {
            GlesVramCommandRectPiece *piece = &output[count++];
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

int GlesVramCommandRectsOverlap(const GlesVramCommandRect *first,
                                const GlesVramCommandRect *second)
{
    if (first == NULL || second == NULL ||
        first->width <= 0 || first->height <= 0 ||
        second->width <= 0 || second->height <= 0)
        return 0;

    return first->x < second->x + second->width &&
           second->x < first->x + first->width &&
           first->y < second->y + second->height &&
           second->y < first->y + first->height;
}

int GlesVramCommandIntersectRects(const GlesVramCommandRect *first,
                                  const GlesVramCommandRect *second,
                                  GlesVramCommandRect *intersection)
{
    int x0;
    int y0;
    int x1;
    int y1;

    if (intersection == NULL ||
        !GlesVramCommandRectsOverlap(first, second))
        return 0;

    x0 = first->x > second->x ? first->x : second->x;
    y0 = first->y > second->y ? first->y : second->y;
    x1 = first->x + first->width < second->x + second->width ?
         first->x + first->width : second->x + second->width;
    y1 = first->y + first->height < second->y + second->height ?
         first->y + first->height : second->y + second->height;

    intersection->x = x0;
    intersection->y = y0;
    intersection->width = x1 - x0;
    intersection->height = y1 - y0;
    return 1;
}

static int GlesVramCommandRectIsValid(
    const GlesVramCommandRect *rect)
{
    return rect != NULL &&
           rect->x >= 0 && rect->y >= 0 &&
           rect->width > 0 && rect->height > 0 &&
           rect->x + rect->width <= GLES_VRAM_COMMAND_WIDTH &&
           rect->y + rect->height <= GLES_VRAM_COMMAND_HEIGHT;
}

static int GlesVramCommandRectsEqual(
    const GlesVramCommandRect *first,
    const GlesVramCommandRect *second)
{
    return first->x == second->x &&
           first->y == second->y &&
           first->width == second->width &&
           first->height == second->height;
}

int GlesVramCommandSelectUnmappedDisplayPageWrite(
    const GlesVramCommandRect *writeRect,
    const GlesVramCommandRect *currentPage,
    const GlesVramCommandRect *previousPage,
    GlesVramCommandRect *candidateRect)
{
    int writeMatchesDisplaySize;

    if (!GlesVramCommandRectIsValid(writeRect) ||
        !GlesVramCommandRectIsValid(currentPage) ||
        !GlesVramCommandRectIsValid(previousPage) ||
        candidateRect == NULL)
        return 0;

    writeMatchesDisplaySize =
        (writeRect->width == currentPage->width &&
         writeRect->height == currentPage->height) ||
        (writeRect->width == previousPage->width &&
         writeRect->height == previousPage->height);
    if (!writeMatchesDisplaySize)
        return 0;

    if (writeRect->width == GLES_VRAM_COMMAND_WIDTH &&
        writeRect->height == GLES_VRAM_COMMAND_HEIGHT)
        return 0;

    if (GlesVramCommandRectsEqual(writeRect, currentPage) ||
        GlesVramCommandRectsEqual(writeRect, previousPage))
        return 0;

    *candidateRect = *writeRect;
    return 1;
}

int GlesVramCommandSelectUnmappedFullDrawPage(
    const GlesVramCommandRect *writeRect,
    const GlesVramCommandRect *drawPage,
    int drawOffsetX,
    int drawOffsetY,
    const GlesVramCommandRect *currentPage,
    const GlesVramCommandRect *previousPage,
    GlesVramCommandRect *uploadRect)
{
    GlesVramCommandRect candidateRect;

    if (!GlesVramCommandRectIsValid(drawPage) || uploadRect == NULL ||
        !GlesVramCommandSelectUnmappedDisplayPageWrite(
            writeRect, currentPage, previousPage, &candidateRect))
        return 0;

    /* The fallback is intentionally limited to the exact command shape seen
     * during a page transition: A0 replaces one complete drawing page and
     * E5 maps primitive coordinates to the same page origin.  A larger A0 or
     * a mismatched offset is ambiguous and remains on the normal path. */
    if (!GlesVramCommandRectsEqual(&candidateRect, drawPage) ||
        drawOffsetX != drawPage->x ||
        drawOffsetY != drawPage->y)
        return 0;

    *uploadRect = *drawPage;
    return 1;
}
