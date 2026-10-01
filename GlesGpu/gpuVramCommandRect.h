#ifndef GPU_VRAM_COMMAND_RECT_H
#define GPU_VRAM_COMMAND_RECT_H

#include <stdint.h>

#define GLES_VRAM_COMMAND_WIDTH 1024
#define GLES_VRAM_COMMAND_HEIGHT 512
#define GLES_VRAM_COMMAND_MAX_WRAP_PIECES 4

typedef struct GlesVramCommandRectTag
{
    int x;
    int y;
    int width;
    int height;
} GlesVramCommandRect;

typedef struct GlesVramCommandRectPieceTag
{
    GlesVramCommandRect rect;
    int commandX;
    int commandY;
} GlesVramCommandRectPiece;

void GlesVramCommandDecodeTransferSize(uint32_t sizeWord,
                                       int *width, int *height);

int GlesVramCommandDecodeFill(uint32_t positionWord, uint32_t sizeWord,
                              GlesVramCommandRect *rect);

int GlesVramCommandSplitWrappedRect(
    const GlesVramCommandRect *input,
    GlesVramCommandRectPiece output[
        GLES_VRAM_COMMAND_MAX_WRAP_PIECES]);

int GlesVramCommandRectsOverlap(const GlesVramCommandRect *first,
                                const GlesVramCommandRect *second);

int GlesVramCommandIntersectRects(const GlesVramCommandRect *first,
                                  const GlesVramCommandRect *second,
                                  GlesVramCommandRect *intersection);

int GlesVramCommandSelectUnmappedDisplayPageWrite(
    const GlesVramCommandRect *writeRect,
    const GlesVramCommandRect *currentPage,
    const GlesVramCommandRect *previousPage,
    GlesVramCommandRect *candidateRect);

int GlesVramCommandSelectUnmappedFullDrawPage(
    const GlesVramCommandRect *writeRect,
    const GlesVramCommandRect *drawPage,
    int drawOffsetX,
    int drawOffsetY,
    const GlesVramCommandRect *currentPage,
    const GlesVramCommandRect *previousPage,
    GlesVramCommandRect *uploadRect);

#endif
