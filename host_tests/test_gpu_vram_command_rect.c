#include <stdint.h>
#include <stdio.h>

#include "../GlesGpu/gpuVramCommandRect.h"

static int failures;

static void expect(int condition, const char *message)
{
    if (!condition)
    {
        printf("FAIL %s\n", message);
        failures++;
    }
}

static int PieceArea(const GlesVramCommandRectPiece *piece)
{
    return piece->rect.width * piece->rect.height;
}

static uint32_t NextRandom(uint32_t *state)
{
    *state = *state * 1664525u + 1013904223u;
    return *state;
}

static int NormalizeCoordinate(int value, int limit)
{
    int normalized = value % limit;
    return normalized < 0 ? normalized + limit : normalized;
}

static void TestTransferSize(void)
{
    int width = 0;
    int height = 0;

    GlesVramCommandDecodeTransferSize(0, &width, &height);
    expect(width == 1024 && height == 512,
           "zero transfer extent means full VRAM");

    GlesVramCommandDecodeTransferSize((240u << 16) | 320u,
                                      &width, &height);
    expect(width == 320 && height == 240,
           "ordinary transfer extent is preserved");

    GlesVramCommandDecodeTransferSize((0x200u << 16) | 0x400u,
                                      &width, &height);
    expect(width == 1024 && height == 512,
           "masked full-size extent decodes from raw limit bits");

    GlesVramCommandDecodeTransferSize(0xffffffffu, &width, &height);
    expect(width == 1023 && height == 511,
           "transfer extent ignores bits outside the hardware fields");
}

static void TestSimpleAndNegativeWrap(void)
{
    GlesVramCommandRect rect = {100, 20, 200, 30};
    GlesVramCommandRectPiece piece[
        GLES_VRAM_COMMAND_MAX_WRAP_PIECES];
    int count;

    count = GlesVramCommandSplitWrappedRect(&rect, piece);
    expect(count == 1, "ordinary rectangle produces one piece");
    expect(piece[0].rect.x == 100 && piece[0].rect.y == 20 &&
           piece[0].rect.width == 200 && piece[0].rect.height == 30 &&
           piece[0].commandX == 0 && piece[0].commandY == 0,
           "ordinary rectangle preserves position and command offset");

    rect.x = -4;
    rect.y = -2;
    rect.width = 8;
    rect.height = 4;
    count = GlesVramCommandSplitWrappedRect(&rect, piece);
    expect(count == 4, "negative coordinate can wrap on both axes");
    expect(piece[0].rect.x == 1020 && piece[0].rect.y == 510 &&
           piece[0].rect.width == 4 && piece[0].rect.height == 2,
           "negative coordinate normalizes to the final VRAM corner");
    expect(piece[3].rect.x == 0 && piece[3].rect.y == 0 &&
           piece[3].rect.width == 4 && piece[3].rect.height == 2 &&
           piece[3].commandX == 4 && piece[3].commandY == 2,
           "negative dual wrap preserves command offsets");
}

static void TestDualAxisWrap(void)
{
    GlesVramCommandRect rect = {1000, 500, 40, 20};
    GlesVramCommandRectPiece piece[
        GLES_VRAM_COMMAND_MAX_WRAP_PIECES];
    int count = GlesVramCommandSplitWrappedRect(&rect, piece);
    int area = 0;
    int index;

    expect(count == 4, "X and Y wrap produces four pieces");
    expect(piece[0].rect.x == 1000 && piece[0].rect.y == 500 &&
           piece[0].rect.width == 24 && piece[0].rect.height == 12 &&
           piece[0].commandX == 0 && piece[0].commandY == 0,
           "dual wrap first piece is correct");
    expect(piece[1].rect.x == 0 && piece[1].rect.y == 500 &&
           piece[1].rect.width == 16 && piece[1].rect.height == 12 &&
           piece[1].commandX == 24 && piece[1].commandY == 0,
           "dual wrap X continuation is correct");
    expect(piece[2].rect.x == 1000 && piece[2].rect.y == 0 &&
           piece[2].rect.width == 24 && piece[2].rect.height == 8 &&
           piece[2].commandX == 0 && piece[2].commandY == 12,
           "dual wrap Y continuation is correct");
    expect(piece[3].rect.x == 0 && piece[3].rect.y == 0 &&
           piece[3].rect.width == 16 && piece[3].rect.height == 8 &&
           piece[3].commandX == 24 && piece[3].commandY == 12,
           "dual wrap final piece is correct");

    for (index = 0; index < count; index++)
        area += PieceArea(&piece[index]);
    expect(area == rect.width * rect.height,
           "wrapped pieces preserve total area");
}

static void TestFullVramFromOffset(void)
{
    GlesVramCommandRect rect = {100, 200, 1024, 512};
    GlesVramCommandRectPiece piece[
        GLES_VRAM_COMMAND_MAX_WRAP_PIECES];
    int count = GlesVramCommandSplitWrappedRect(&rect, piece);
    int area = 0;
    int index;

    expect(count == 4,
           "full VRAM transfer from an offset wraps on both axes");
    for (index = 0; index < count; index++)
        area += PieceArea(&piece[index]);
    expect(area == 1024 * 512,
           "full VRAM wrapped pieces preserve complete area");

    rect.width = 1025;
    expect(GlesVramCommandSplitWrappedRect(&rect, piece) == 0,
           "rectangle wider than VRAM is rejected");
    rect.width = 1024;
    rect.height = 513;
    expect(GlesVramCommandSplitWrappedRect(&rect, piece) == 0,
           "rectangle taller than VRAM is rejected");
}

static void TestFillDecode(void)
{
    GlesVramCommandRect rect;
    GlesVramCommandRectPiece piece[
        GLES_VRAM_COMMAND_MAX_WRAP_PIECES];
    uint32_t position = (500u << 16) | 1009u;
    uint32_t size = (20u << 16) | 17u;
    int count;

    expect(GlesVramCommandDecodeFill(position, size, &rect),
           "non-empty Fill decodes");
    expect(rect.x == 1008 && rect.y == 500 &&
           rect.width == 32 && rect.height == 20,
           "Fill aligns X down and width up to 16 pixels");
    count = GlesVramCommandSplitWrappedRect(&rect, piece);
    expect(count == 4,
           "aligned Fill can wrap across right and bottom edges");

    expect(!GlesVramCommandDecodeFill(position, 20u << 16, &rect),
           "zero Fill width remains an empty operation");
    expect(!GlesVramCommandDecodeFill(position, 17u, &rect),
           "zero Fill height remains an empty operation");

    size = (1u << 16) | 1023u;
    expect(GlesVramCommandDecodeFill(0, size, &rect) &&
           rect.width == 1024,
           "maximum Fill width rounds to the full VRAM width");
}

static void TestOverlapAndIntersection(void)
{
    GlesVramCommandRect first = {10, 20, 30, 40};
    GlesVramCommandRect second = {39, 59, 10, 10};
    GlesVramCommandRect intersection;

    expect(GlesVramCommandRectsOverlap(&first, &second),
           "one-pixel overlap is detected");
    expect(GlesVramCommandIntersectRects(&first, &second,
                                         &intersection),
           "overlap produces an intersection");
    expect(intersection.x == 39 && intersection.y == 59 &&
           intersection.width == 1 && intersection.height == 1,
           "intersection uses half-open bounds");

    second.x = 40;
    second.y = 20;
    expect(!GlesVramCommandRectsOverlap(&first, &second),
           "touching horizontal edges do not overlap");
    expect(!GlesVramCommandIntersectRects(&first, &second,
                                          &intersection),
           "disjoint rectangles have no intersection");

    second.x = 10;
    second.y = 60;
    expect(!GlesVramCommandRectsOverlap(&first, &second),
           "touching vertical edges do not overlap");
}

static void TestUnmappedFullDrawPage(void)
{
    GlesVramCommandRect write = {0, 240, 512, 240};
    GlesVramCommandRect draw = {0, 240, 512, 240};
    GlesVramCommandRect current = {0, 0, 512, 240};
    GlesVramCommandRect previous = {0, 2, 512, 240};
    GlesVramCommandRect upload = {-1, -1, -1, -1};

    expect(GlesVramCommandSelectUnmappedDisplayPageWrite(
               &write, &current, &previous, &upload),
           "full A0 is retained before DrawArea selects its target page");
    expect(upload.x == 0 && upload.y == 240 &&
           upload.width == 512 && upload.height == 240,
           "deferred A0 candidate preserves the complete page");

    expect(GlesVramCommandSelectUnmappedFullDrawPage(
               &write, &draw, 0, 240,
               &current, &previous, &upload),
           "full A0 selects a DrawArea page missed by stale display maps");
    expect(upload.x == 0 && upload.y == 240 &&
           upload.width == 512 && upload.height == 240,
           "unmapped upload uses the complete DrawArea page");

    write.height = 239;
    expect(!GlesVramCommandSelectUnmappedDisplayPageWrite(
                &write, &current, &previous, &upload),
           "a partial A0 is not retained as a display page");
    expect(!GlesVramCommandSelectUnmappedFullDrawPage(
                &write, &draw, 0, 240,
                &current, &previous, &upload),
           "a partial A0 cannot replace a complete DrawArea page");

    write.y = 238;
    write.height = 244;
    expect(!GlesVramCommandSelectUnmappedFullDrawPage(
                &write, &draw, 0, 240,
                &current, &previous, &upload),
           "an A0 larger than the DrawArea stays on the normal path");

    write.y = 240;
    write.height = 240;
    expect(!GlesVramCommandSelectUnmappedFullDrawPage(
                &write, &draw, 0, 0,
                &current, &previous, &upload),
           "a mismatched DrawOffset cannot identify a display page");

    write.height = 240;
    previous = draw;
    expect(!GlesVramCommandSelectUnmappedDisplayPageWrite(
                &write, &current, &previous, &upload),
           "an A0 already mapped as previous is not deferred");
    expect(!GlesVramCommandSelectUnmappedFullDrawPage(
                &write, &draw, 0, 240,
                &current, &previous, &upload),
           "a page already mapped as previous stays on the legacy path");

    previous.x = 0;
    previous.y = 2;
    previous.width = 512;
    previous.height = 240;
    current = draw;
    expect(!GlesVramCommandSelectUnmappedDisplayPageWrite(
                &write, &current, &previous, &upload),
           "an A0 already mapped as current is not deferred");
    expect(!GlesVramCommandSelectUnmappedFullDrawPage(
                &write, &draw, 0, 240,
                &current, &previous, &upload),
           "a page already mapped as current stays on the legacy path");

    current.x = 0;
    current.y = 0;
    current.width = 640;
    current.height = 240;
    previous.width = 640;
    expect(!GlesVramCommandSelectUnmappedDisplayPageWrite(
                &write, &current, &previous, &upload),
           "an A0 with an unrelated size is not deferred");
    expect(!GlesVramCommandSelectUnmappedFullDrawPage(
                &write, &draw, 0, 240,
                &current, &previous, &upload),
           "an unrelated DrawArea size is not treated as a display page");

    write.x = 0;
    write.y = 0;
    write.width = 1024;
    write.height = 512;
    draw = write;
    current = write;
    current.y = 0;
    previous = current;
    previous.x = 0;
    expect(!GlesVramCommandSelectUnmappedDisplayPageWrite(
                &write, &current, &previous, &upload),
           "a full-VRAM A0 is not retained as a display page");
    expect(!GlesVramCommandSelectUnmappedFullDrawPage(
                &write, &draw, 0, 0,
                &current, &previous, &upload),
           "the default full-VRAM DrawArea is never a display-page hint");
}

static void TestRandomWrappedRectProperties(void)
{
    uint32_t random = 0x51a7c0deu;
    int iteration;

    for (iteration = 0; iteration < 20000; iteration++)
    {
        GlesVramCommandRect rect;
        GlesVramCommandRectPiece piece[
            GLES_VRAM_COMMAND_MAX_WRAP_PIECES];
        int count;
        int area = 0;
        int first;

        rect.x = (int)(NextRandom(&random) % 4097u) - 2048;
        rect.y = (int)(NextRandom(&random) % 2049u) - 1024;
        rect.width = (int)(NextRandom(&random) % 1024u) + 1;
        rect.height = (int)(NextRandom(&random) % 512u) + 1;
        count = GlesVramCommandSplitWrappedRect(&rect, piece);

        expect(count >= 1 && count <= 4,
               "valid wrapped rectangle produces one to four pieces");
        for (first = 0; first < count; first++)
        {
            const GlesVramCommandRectPiece *current = &piece[first];
            int second;

            expect(current->rect.x >= 0 && current->rect.y >= 0 &&
                   current->rect.width > 0 && current->rect.height > 0 &&
                   current->rect.x + current->rect.width <= 1024 &&
                   current->rect.y + current->rect.height <= 512,
                   "wrapped piece remains inside physical VRAM");
            expect(current->commandX >= 0 && current->commandY >= 0 &&
                   current->commandX + current->rect.width <= rect.width &&
                   current->commandY + current->rect.height <= rect.height,
                   "wrapped piece remains inside command coordinates");
            expect(NormalizeCoordinate(rect.x + current->commandX,
                                       1024) == current->rect.x &&
                   NormalizeCoordinate(rect.y + current->commandY,
                                       512) == current->rect.y,
                   "wrapped piece maps back to its command offset");
            area += PieceArea(current);

            for (second = first + 1; second < count; second++)
                expect(!GlesVramCommandRectsOverlap(
                           &current->rect, &piece[second].rect),
                       "wrapped physical pieces do not overlap");
        }
        expect(area == rect.width * rect.height,
               "random wrapped pieces preserve total area");
    }
}

int main(void)
{
    TestTransferSize();
    TestSimpleAndNegativeWrap();
    TestDualAxisWrap();
    TestFullVramFromOffset();
    TestFillDecode();
    TestOverlapAndIntersection();
    TestUnmappedFullDrawPage();
    TestRandomWrappedRectProperties();

    if (failures == 0)
        printf("PASS gpu_vram_command_rect\n");
    return failures != 0;
}
