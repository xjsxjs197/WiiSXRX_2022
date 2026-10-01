#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../GlesGpu/gpuVramCommandRect.h"
#include "../GlesGpu/gpuVramCommandTransfer.h"

static int failures;

static uint32_t NextRandom(uint32_t *state)
{
    *state = *state * 1664525u + 1013904223u;
    return *state;
}

static void expect(int condition, const char *message)
{
    if (!condition)
    {
        printf("FAIL %s\n", message);
        failures++;
    }
}

static uint16_t *NewVram(void)
{
    return (uint16_t *)calloc(
        GLES_VRAM_COMMAND_WIDTH * GLES_VRAM_COMMAND_HEIGHT,
        sizeof(uint16_t));
}

static void TestLittleEndianAndMaskWrite(void)
{
    uint16_t *vram = NewVram();
    unsigned char *bytes = (unsigned char *)vram;

    expect(vram != NULL, "VRAM allocation succeeds");
    if (vram == NULL)
        return;

    expect(GlesVramCommandWritePixel(vram, 0, 0, 0x1234, 0, 0),
           "ordinary pixel write succeeds");
    expect(bytes[0] == 0x34 && bytes[1] == 0x12,
           "pixel backing is always little endian");
    expect(GlesVramCommandReadPixel(vram, 0, 0) == 0x1234,
           "little-endian pixel reads back in host order");

    expect(GlesVramCommandWritePixel(vram, 1, 0, 0x0234, 1, 0) &&
           GlesVramCommandReadPixel(vram, 1, 0) == 0x8234,
           "set-mask adds bit 15");
    expect(!GlesVramCommandWritePixel(vram, 1, 0, 0x1111, 0, 1) &&
           GlesVramCommandReadPixel(vram, 1, 0) == 0x8234,
           "check-mask preserves a masked destination");
    expect(GlesVramCommandWritePixel(vram, 2, 0, 0x1111, 1, 1) &&
           GlesVramCommandReadPixel(vram, 2, 0) == 0x9111,
           "set-mask and check-mask write an unmasked destination");

    expect(GlesVramCommandWritePixel(vram, 1024, 512,
                                     0x4567, 0, 0) &&
           GlesVramCommandReadPixel(vram, 0, 0) == 0x4567,
           "pixel access wraps on both axes");
    free(vram);
}

static void TestFillWrap(void)
{
    uint16_t *vram = NewVram();

    expect(vram != NULL, "Fill VRAM allocation succeeds");
    if (vram == NULL)
        return;
    expect(GlesVramCommandFill(vram, 1022, 510, 4, 4, 0x3456),
           "wrapped Fill succeeds");
    expect(GlesVramCommandReadPixel(vram, 1022, 510) == 0x3456 &&
           GlesVramCommandReadPixel(vram, 1, 510) == 0x3456 &&
           GlesVramCommandReadPixel(vram, 1022, 1) == 0x3456 &&
           GlesVramCommandReadPixel(vram, 1, 1) == 0x3456,
           "Fill writes all four wrapped corners");
    expect(GlesVramCommandReadPixel(vram, 2, 2) == 0,
           "Fill does not write outside its wrapped rectangle");
    free(vram);
}

static void TestReadTransferWord(void)
{
    uint16_t *vram = NewVram();
    uint32_t value = 0xffffffffu;

    expect(vram != NULL, "read-word VRAM allocation succeeds");
    if (vram == NULL)
        return;

    GlesVramCommandWritePixel(vram, 1023, 511, 0x1111, 0, 0);
    GlesVramCommandWritePixel(vram, 0, 511, 0x2222, 0, 0);
    GlesVramCommandWritePixel(vram, 1, 511, 0x3333, 0, 0);
    GlesVramCommandWritePixel(vram, 2, 511, 0xaaaa, 0, 0);
    expect(GlesVramCommandReadTransferWord(
               vram, 1023, 511, 3, 1, 0, &value) == 2 &&
           value == 0x22221111u,
           "read word follows row-major transfer wrapping");
    expect(GlesVramCommandReadTransferWord(
               vram, 1023, 511, 3, 1, 2, &value) == 1 &&
           value == 0x00003333u,
           "odd final GPUREAD halfword is zero-filled");
    expect(GlesVramCommandReadTransferWord(
               vram, 1023, 511, 3, 1, 3, &value) == 0,
           "read word rejects an offset after the transfer");
    free(vram);
}

static void SeedRow(uint16_t *vram, int y)
{
    int x;
    for (x = 0; x < 16; x++)
        GlesVramCommandWritePixel(vram, x, y, (uint16_t)(x + 1), 0, 0);
}

static void TestHorizontalOverlapOrder(void)
{
    uint16_t *vram = NewVram();

    expect(vram != NULL, "overlap VRAM allocation succeeds");
    if (vram == NULL)
        return;

    SeedRow(vram, 0);
    expect(GlesVramCommandCopy(vram, 0, 0, 2, 0, 6, 1, 0, 0),
           "right-moving overlap copy succeeds");
    expect(GlesVramCommandReadPixel(vram, 2, 0) == 1 &&
           GlesVramCommandReadPixel(vram, 3, 0) == 2 &&
           GlesVramCommandReadPixel(vram, 7, 0) == 6,
           "right-moving overlap copies columns in reverse");

    memset(vram, 0, GLES_VRAM_COMMAND_WIDTH *
                    GLES_VRAM_COMMAND_HEIGHT * sizeof(uint16_t));
    SeedRow(vram, 0);
    expect(GlesVramCommandCopy(vram, 2, 0, 0, 0, 6, 1, 0, 0),
           "left-moving overlap copy succeeds");
    expect(GlesVramCommandReadPixel(vram, 0, 0) == 3 &&
           GlesVramCommandReadPixel(vram, 1, 0) == 4 &&
           GlesVramCommandReadPixel(vram, 5, 0) == 8,
           "left-moving overlap copies columns forward");
    free(vram);
}

static void TestVerticalForwardOrder(void)
{
    uint16_t *vram = NewVram();

    expect(vram != NULL, "vertical overlap VRAM allocation succeeds");
    if (vram == NULL)
        return;
    GlesVramCommandWritePixel(vram, 0, 0, 0x0011, 0, 0);
    GlesVramCommandWritePixel(vram, 0, 1, 0x0022, 0, 0);
    GlesVramCommandWritePixel(vram, 0, 2, 0x0033, 0, 0);
    expect(GlesVramCommandCopy(vram, 0, 0, 0, 1, 1, 3, 0, 0),
           "vertical overlap copy succeeds");
    expect(GlesVramCommandReadPixel(vram, 0, 1) == 0x0011 &&
           GlesVramCommandReadPixel(vram, 0, 2) == 0x0011 &&
           GlesVramCommandReadPixel(vram, 0, 3) == 0x0011,
           "VRAM copy processes rows forward");
    free(vram);
}

static void TestCopyWrapAndMask(void)
{
    uint16_t *vram = NewVram();

    expect(vram != NULL, "wrapped copy VRAM allocation succeeds");
    if (vram == NULL)
        return;

    GlesVramCommandWritePixel(vram, 1023, 511, 0x0101, 0, 0);
    GlesVramCommandWritePixel(vram, 0, 511, 0x0202, 0, 0);
    GlesVramCommandWritePixel(vram, 1023, 0, 0x0303, 0, 0);
    GlesVramCommandWritePixel(vram, 0, 0, 0x0404, 0, 0);
    expect(GlesVramCommandCopy(vram, 1023, 511, 10, 20,
                               2, 2, 1, 0),
           "source-wrapped masked copy succeeds");
    expect(GlesVramCommandReadPixel(vram, 10, 20) == 0x8101 &&
           GlesVramCommandReadPixel(vram, 11, 20) == 0x8202 &&
           GlesVramCommandReadPixel(vram, 10, 21) == 0x8303 &&
           GlesVramCommandReadPixel(vram, 11, 21) == 0x8404,
           "copy preserves source offsets across both wrapped axes");

    GlesVramCommandWritePixel(vram, 30, 40, 0x8aaa, 0, 0);
    GlesVramCommandWritePixel(vram, 31, 40, 0x0bbb, 0, 0);
    expect(GlesVramCommandCopy(vram, 10, 20, 30, 40,
                               2, 1, 0, 1),
           "check-mask copy succeeds");
    expect(GlesVramCommandReadPixel(vram, 30, 40) == 0x8aaa &&
           GlesVramCommandReadPixel(vram, 31, 40) == 0x8202,
           "check-mask skips only masked destination pixels");
    free(vram);
}

static void TestSameTargetSetMask(void)
{
    uint16_t *vram = NewVram();

    expect(vram != NULL, "same-target VRAM allocation succeeds");
    if (vram == NULL)
        return;
    GlesVramCommandWritePixel(vram, 7, 9, 0x1234, 0, 0);
    expect(GlesVramCommandCopy(vram, 7, 9, 7, 9,
                               1, 1, 1, 0) &&
           GlesVramCommandReadPixel(vram, 7, 9) == 0x9234,
           "same source and target still applies set-mask");
    free(vram);
}

static uint16_t ReferenceRead(const uint16_t *vram, int x, int y)
{
    const unsigned char *bytes = (const unsigned char *)vram;
    int index = ((y & 511) * 1024 + (x & 1023)) * 2;
    return (uint16_t)bytes[index] | ((uint16_t)bytes[index + 1] << 8);
}

static void ReferenceWrite(uint16_t *vram, int x, int y, uint16_t value,
                           int setMask, int checkMask)
{
    unsigned char *bytes = (unsigned char *)vram;
    int index = ((y & 511) * 1024 + (x & 1023)) * 2;
    uint16_t old = (uint16_t)bytes[index] |
                   ((uint16_t)bytes[index + 1] << 8);

    if (checkMask && (old & 0x8000u) != 0)
        return;
    if (setMask)
        value |= 0x8000u;
    bytes[index] = (unsigned char)value;
    bytes[index + 1] = (unsigned char)(value >> 8);
}

static void ReferenceCopyChunk(uint16_t *vram,
                               int sourceX, int sourceY,
                               int destinationX, int destinationY,
                               int width, int height,
                               int setMask, int checkMask)
{
    int reverse = sourceX < destinationX ||
                  ((sourceX + width - 1) & 1023) <
                  ((destinationX + width - 1) & 1023);
    int row;

    for (row = 0; row < height; row++)
    {
        int column = reverse ? width - 1 : 0;
        int end = reverse ? -1 : width;
        int step = reverse ? -1 : 1;
        for (; column != end; column += step)
        {
            uint16_t source = ReferenceRead(
                vram, sourceX + column, sourceY + row);
            ReferenceWrite(vram,
                           destinationX + column, destinationY + row,
                           source, setMask, checkMask);
        }
    }
}

static void ReferenceCopy(uint16_t *vram,
                          int sourceX, int sourceY,
                          int destinationX, int destinationY,
                          int width, int height,
                          int setMask, int checkMask)
{
    int remainingRows = height;

    sourceX &= 1023;
    sourceY &= 511;
    destinationX &= 1023;
    destinationY &= 511;
    if (sourceX + width <= 1024 && destinationX + width <= 1024)
    {
        ReferenceCopyChunk(vram, sourceX, sourceY,
                           destinationX, destinationY,
                           width, height, setMask, checkMask);
        return;
    }

    while (remainingRows > 0)
    {
        int rows = remainingRows;
        int sourceRows = 512 - sourceY;
        int destinationRows = 512 - destinationY;
        int remainingColumns = width;
        int currentSourceX = sourceX;
        int currentDestinationX = destinationX;

        if (rows > sourceRows) rows = sourceRows;
        if (rows > destinationRows) rows = destinationRows;
        while (remainingColumns > 0)
        {
            int columns = remainingColumns;
            int sourceColumns = 1024 - currentSourceX;
            int destinationColumns = 1024 - currentDestinationX;
            if (columns > sourceColumns) columns = sourceColumns;
            if (columns > destinationColumns) columns = destinationColumns;
            ReferenceCopyChunk(vram,
                               currentSourceX, sourceY,
                               currentDestinationX, destinationY,
                               columns, rows, setMask, checkMask);
            currentSourceX = (currentSourceX + columns) & 1023;
            currentDestinationX =
                (currentDestinationX + columns) & 1023;
            remainingColumns -= columns;
        }
        sourceY = (sourceY + rows) & 511;
        destinationY = (destinationY + rows) & 511;
        remainingRows -= rows;
    }
}

static void TestRandomCopyDifferential(void)
{
    const size_t bytes = 1024u * 512u * sizeof(uint16_t);
    uint16_t *actual = NewVram();
    uint16_t *expected = NewVram();
    uint32_t random = 0xc001d00du;
    int iteration;

    expect(actual != NULL && expected != NULL,
           "random differential VRAM allocation succeeds");
    if (actual == NULL || expected == NULL)
    {
        free(actual);
        free(expected);
        return;
    }

    for (iteration = 0; iteration < 64; iteration++)
    {
        int sourceX;
        int sourceY;
        int destinationX;
        int destinationY;
        int width;
        int height;
        int setMask;
        int checkMask;
        int pixel;

        for (pixel = 0; pixel < 1024 * 512; pixel++)
        {
            uint16_t value = (uint16_t)(pixel * 40503u +
                                        iteration * 7919u);
            ReferenceWrite(expected, pixel & 1023, pixel >> 10,
                           value, 0, 0);
        }
        memcpy(actual, expected, bytes);

        sourceX = (int)(NextRandom(&random) & 1023u);
        sourceY = (int)(NextRandom(&random) & 511u);
        destinationX = (int)(NextRandom(&random) & 1023u);
        destinationY = (int)(NextRandom(&random) & 511u);
        width = (int)(NextRandom(&random) % 96u) + 1;
        height = (int)(NextRandom(&random) % 12u) + 1;
        if ((iteration & 3) == 0)
            sourceX = 1024 - width / 2;
        else if ((iteration & 3) == 1)
            destinationX = 1024 - width / 2;
        if ((iteration & 7) == 2)
            sourceY = 510;
        if ((iteration & 7) == 3)
            destinationY = 510;
        if ((iteration & 7) >= 4)
        {
            sourceX = 400;
            destinationX = 400 + (iteration & 15);
            sourceY = destinationY = 100;
        }
        setMask = iteration & 1;
        checkMask = (iteration >> 1) & 1;

        ReferenceCopy(expected, sourceX, sourceY,
                      destinationX, destinationY,
                      width, height, setMask, checkMask);
        GlesVramCommandCopy(actual, sourceX, sourceY,
                            destinationX, destinationY,
                            width, height, setMask, checkMask);
        if (memcmp(actual, expected, bytes) != 0)
        {
            expect(0, "random Move matches the independent reference");
            break;
        }
    }
    if (iteration == 64)
        expect(1, "random Move matches the independent reference");
    free(actual);
    free(expected);
}

int main(void)
{
    TestLittleEndianAndMaskWrite();
    TestFillWrap();
    TestReadTransferWord();
    TestHorizontalOverlapOrder();
    TestVerticalForwardOrder();
    TestCopyWrapAndMask();
    TestSameTargetSetMask();
    TestRandomCopyDifferential();

    if (failures == 0)
        printf("PASS gpu_vram_command_transfer\n");
    return failures != 0;
}
