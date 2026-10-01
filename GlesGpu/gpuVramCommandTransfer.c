#include "gpuVramCommandTransfer.h"

#include "gpuVramCommandRect.h"

#include <stddef.h>

static uint16_t GlesVramCommandReadLittle16(const uint16_t *pixel)
{
    const unsigned char *bytes = (const unsigned char *)pixel;
    return (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8);
}

static void GlesVramCommandWriteLittle16(uint16_t *pixel, uint16_t value)
{
    unsigned char *bytes = (unsigned char *)pixel;
    bytes[0] = (unsigned char)value;
    bytes[1] = (unsigned char)(value >> 8);
}

uint16_t GlesVramCommandReadPixel(const uint16_t *vram, int x, int y)
{
    int wrappedX;
    int wrappedY;

    if (vram == NULL)
        return 0;
    wrappedX = x & (GLES_VRAM_COMMAND_WIDTH - 1);
    wrappedY = y & (GLES_VRAM_COMMAND_HEIGHT - 1);
    return GlesVramCommandReadLittle16(
        vram + wrappedY * GLES_VRAM_COMMAND_WIDTH + wrappedX);
}

int GlesVramCommandReadTransferWord(const uint16_t *vram,
                                    int x, int y,
                                    int width, int height,
                                    int pixelOffset,
                                    uint32_t *value)
{
    int totalPixels;
    int row;
    int column;
    uint32_t packed;

    if (vram == NULL || value == NULL ||
        width <= 0 || height <= 0 ||
        width > GLES_VRAM_COMMAND_WIDTH ||
        height > GLES_VRAM_COMMAND_HEIGHT)
        return 0;

    totalPixels = width * height;
    if (pixelOffset < 0 || pixelOffset >= totalPixels)
        return 0;

    row = pixelOffset / width;
    column = pixelOffset % width;
    packed = GlesVramCommandReadPixel(vram, x + column, y + row);
    if (pixelOffset + 1 < totalPixels)
    {
        column++;
        if (column == width)
        {
            column = 0;
            row++;
        }
        packed |= (uint32_t)GlesVramCommandReadPixel(
            vram, x + column, y + row) << 16;
        *value = packed;
        return 2;
    }

    /* GPUREAD zero-fills the unused half of the last word. */
    *value = packed;
    return 1;
}

int GlesVramCommandWritePixel(uint16_t *vram, int x, int y,
                              uint16_t value, int setMask, int checkMask)
{
    int wrappedX;
    int wrappedY;
    uint16_t *destination;

    if (vram == NULL)
        return 0;
    wrappedX = x & (GLES_VRAM_COMMAND_WIDTH - 1);
    wrappedY = y & (GLES_VRAM_COMMAND_HEIGHT - 1);
    destination = vram + wrappedY * GLES_VRAM_COMMAND_WIDTH + wrappedX;
    if (checkMask &&
        (GlesVramCommandReadLittle16(destination) & 0x8000u) != 0)
        return 0;
    if (setMask)
        value |= 0x8000u;
    GlesVramCommandWriteLittle16(destination, value);
    return 1;
}

int GlesVramCommandFill(uint16_t *vram, int x, int y,
                        int width, int height, uint16_t value)
{
    int row;
    int column;

    if (vram == NULL || width <= 0 || height <= 0 ||
        width > GLES_VRAM_COMMAND_WIDTH ||
        height > GLES_VRAM_COMMAND_HEIGHT)
        return 0;

    for (row = 0; row < height; row++)
    {
        for (column = 0; column < width; column++)
        {
            GlesVramCommandWritePixel(vram, x + column, y + row,
                                      value, 0, 0);
        }
    }
    return 1;
}

static void GlesVramCommandCopyChunk(uint16_t *vram,
                                     int sourceX, int sourceY,
                                     int destinationX, int destinationY,
                                     int width, int height,
                                     int setMask, int checkMask)
{
    int reverse;
    int row;

    reverse = sourceX < destinationX ||
              ((sourceX + width - 1) &
               (GLES_VRAM_COMMAND_WIDTH - 1)) <
              ((destinationX + width - 1) &
               (GLES_VRAM_COMMAND_WIDTH - 1));

    for (row = 0; row < height; row++)
    {
        int column;

        if (reverse)
        {
            for (column = width - 1; column >= 0; column--)
            {
                uint16_t source = GlesVramCommandReadPixel(
                    vram, sourceX + column, sourceY + row);
                GlesVramCommandWritePixel(
                    vram, destinationX + column, destinationY + row,
                    source, setMask, checkMask);
            }
        }
        else
        {
            for (column = 0; column < width; column++)
            {
                uint16_t source = GlesVramCommandReadPixel(
                    vram, sourceX + column, sourceY + row);
                GlesVramCommandWritePixel(
                    vram, destinationX + column, destinationY + row,
                    source, setMask, checkMask);
            }
        }
    }
}

int GlesVramCommandCopy(uint16_t *vram,
                        int sourceX, int sourceY,
                        int destinationX, int destinationY,
                        int width, int height,
                        int setMask, int checkMask)
{
    if (vram == NULL || width <= 0 || height <= 0 ||
        width > GLES_VRAM_COMMAND_WIDTH ||
        height > GLES_VRAM_COMMAND_HEIGHT)
        return 0;

    sourceX &= GLES_VRAM_COMMAND_WIDTH - 1;
    sourceY &= GLES_VRAM_COMMAND_HEIGHT - 1;
    destinationX &= GLES_VRAM_COMMAND_WIDTH - 1;
    destinationY &= GLES_VRAM_COMMAND_HEIGHT - 1;

    if (sourceX + width > GLES_VRAM_COMMAND_WIDTH ||
        destinationX + width > GLES_VRAM_COMMAND_WIDTH)
    {
        int remainingRows = height;
        int currentSourceY = sourceY;
        int currentDestinationY = destinationY;

        while (remainingRows > 0)
        {
            int sourceRows = GLES_VRAM_COMMAND_HEIGHT - currentSourceY;
            int destinationRows =
                GLES_VRAM_COMMAND_HEIGHT - currentDestinationY;
            int rows = remainingRows;
            int remainingColumns = width;
            int currentSourceX = sourceX;
            int currentDestinationX = destinationX;

            if (rows > sourceRows)
                rows = sourceRows;
            if (rows > destinationRows)
                rows = destinationRows;

            while (remainingColumns > 0)
            {
                int sourceColumns =
                    GLES_VRAM_COMMAND_WIDTH - currentSourceX;
                int destinationColumns =
                    GLES_VRAM_COMMAND_WIDTH - currentDestinationX;
                int columns = remainingColumns;

                if (columns > sourceColumns)
                    columns = sourceColumns;
                if (columns > destinationColumns)
                    columns = destinationColumns;
                GlesVramCommandCopyChunk(
                    vram, currentSourceX, currentSourceY,
                    currentDestinationX, currentDestinationY,
                    columns, rows, setMask, checkMask);
                currentSourceX =
                    (currentSourceX + columns) &
                    (GLES_VRAM_COMMAND_WIDTH - 1);
                currentDestinationX =
                    (currentDestinationX + columns) &
                    (GLES_VRAM_COMMAND_WIDTH - 1);
                remainingColumns -= columns;
            }

            currentSourceY =
                (currentSourceY + rows) &
                (GLES_VRAM_COMMAND_HEIGHT - 1);
            currentDestinationY =
                (currentDestinationY + rows) &
                (GLES_VRAM_COMMAND_HEIGHT - 1);
            remainingRows -= rows;
        }
    }
    else
    {
        GlesVramCommandCopyChunk(
            vram, sourceX, sourceY, destinationX, destinationY,
            width, height, setMask, checkMask);
    }
    return 1;
}
