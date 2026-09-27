#include <stdint.h>
#include <stdio.h>

#include "../GlesGpu/gpuVramTiling.h"

static int failures;

static uint32_t NextRandom(uint32_t *state);

static void expect(int condition, const char *message)
{
    if (!condition)
    {
        printf("FAIL %s\n", message);
        failures++;
    }
}

static int SpanArea(const GlesVramTileSpan *span)
{
    return span->vramRect.width * span->vramRect.height;
}

static void TestTransferSize(void)
{
    int width = 0;
    int height = 0;

    GlesVramDecodeTransferSize(0, &width, &height);
    expect(width == 1024 && height == 512,
           "zero transfer extent means full VRAM");
    GlesVramDecodeTransferSize((240u << 16) | 320u, &width, &height);
    expect(width == 320 && height == 240,
           "ordinary transfer extent is preserved");
    GlesVramDecodeTransferSize((0x200u << 16) | 0x400u,
                               &width, &height);
    expect(width == 1024 && height == 512,
           "masked full-size transfer extent is decoded");
}

static void TestSimpleTileSpans(void)
{
    GlesVramRect rect = { 100, 20, 200, 30 };
    GlesVramTileSpan span[GLES_VRAM_MAX_TILE_SPANS];
    int count = GlesVramBuildTileSpans(&rect, span);

    expect(count == 1, "left rectangle has one span");
    expect(span[0].tile == GLES_VRAM_TILE_LEFT &&
           span[0].localRect.x == 100 && span[0].localRect.width == 200,
           "left rectangle keeps absolute local X");

    rect.x = 704;
    rect.width = 64;
    count = GlesVramBuildTileSpans(&rect, span);
    expect(count == 1, "V8 right rectangle has one span");
    expect(span[0].tile == GLES_VRAM_TILE_RIGHT &&
           span[0].localRect.x == 64 && span[0].localRect.width == 64,
           "V8 X=704 maps to right local X=64");
}

static void TestBoundarySplit(void)
{
    GlesVramRect rect = { 620, 10, 80, 20 };
    GlesVramTileSpan span[GLES_VRAM_MAX_TILE_SPANS];
    int count = GlesVramBuildTileSpans(&rect, span);

    expect(count == 2, "cross-boundary rectangle has two spans");
    expect(span[0].tile == GLES_VRAM_TILE_LEFT &&
           span[0].vramRect.x == 620 && span[0].vramRect.width == 20 &&
           span[0].commandX == 0,
           "cross-boundary left portion is correct");
    expect(span[1].tile == GLES_VRAM_TILE_RIGHT &&
           span[1].localRect.x == 0 && span[1].vramRect.width == 60 &&
           span[1].commandX == 20,
           "cross-boundary right portion is correct");

    rect.x = 512;
    rect.width = 512;
    count = GlesVramBuildTileSpans(&rect, span);
    expect(count == 2 && span[0].vramRect.width == 128 &&
           span[1].vramRect.width == 384,
           "Vagrant display X=512 splits into 128+384");
}

static void TestWrapSplits(void)
{
    GlesVramRect rect = { 1000, 500, 40, 20 };
    GlesVramRectPiece piece[GLES_VRAM_MAX_WRAP_PIECES];
    GlesVramTileSpan span[GLES_VRAM_MAX_TILE_SPANS];
    int pieceCount = GlesVramSplitWrappedRect(&rect, piece);
    int spanCount;
    int area = 0;
    int i;

    expect(pieceCount == 4, "X+Y wrap produces four pieces");
    expect(piece[0].rect.x == 1000 && piece[0].rect.y == 500 &&
           piece[0].rect.width == 24 && piece[0].rect.height == 12 &&
           piece[0].commandX == 0 && piece[0].commandY == 0,
           "wrapped first piece is correct");
    expect(piece[1].rect.x == 0 && piece[1].rect.y == 500 &&
           piece[1].rect.width == 16 && piece[1].commandX == 24,
           "wrapped X continuation preserves command offset");
    expect(piece[2].rect.x == 1000 && piece[2].rect.y == 0 &&
           piece[2].rect.height == 8 && piece[2].commandY == 12,
           "wrapped Y continuation preserves command offset");

    spanCount = GlesVramBuildTileSpans(&rect, span);
    for (i = 0; i < spanCount; i++)
        area += SpanArea(&span[i]);
    expect(area == rect.width * rect.height,
           "wrapped tile spans preserve total area");
}

static void TestFullVramFromOffset(void)
{
    GlesVramRect rect = { 100, 200, 1024, 512 };
    GlesVramTileSpan span[GLES_VRAM_MAX_TILE_SPANS];
    int count = GlesVramBuildTileSpans(&rect, span);
    int area = 0;
    int i;

    expect(count == 6,
           "offset full-VRAM rectangle produces six tile spans");
    for (i = 0; i < count; i++)
    {
        area += SpanArea(&span[i]);
        expect(span[i].localRect.x >= 0 && span[i].localRect.y >= 0,
               "full-VRAM span has non-negative local origin");
        if (span[i].tile == GLES_VRAM_TILE_LEFT)
            expect(span[i].localRect.x + span[i].localRect.width <= 640,
                   "left local span stays within 640");
        else
            expect(span[i].localRect.x + span[i].localRect.width <= 384,
                   "right local span stays within 384");
    }
    expect(area == 1024 * 512,
           "offset full-VRAM spans preserve all pixels");
}

static void TestInvalidInput(void)
{
    GlesVramRect rect = { 0, 0, 0, 1 };
    GlesVramTileSpan span[GLES_VRAM_MAX_TILE_SPANS];

    expect(GlesVramBuildTileSpans(&rect, span) == 0,
           "zero width is rejected after extent decoding stage");
    rect.width = 1025;
    expect(GlesVramBuildTileSpans(&rect, span) == 0,
           "width larger than VRAM is rejected");
    rect.width = 1;
    rect.height = 513;
    expect(GlesVramBuildTileSpans(&rect, span) == 0,
           "height larger than VRAM is rejected");
}

static void TestDisplayPlans(void)
{
    GlesVramDisplayPlan plan;
    GlesVramRect rect = {512, 0, 512, 480};

    expect(GlesVramBuildDisplayPlan(&rect, &plan),
           "Vagrant display plan is accepted");
    expect(plan.segmentCount == 2,
           "Vagrant display has two segments");
    expect(plan.segment[0].tile == GLES_VRAM_TILE_LEFT &&
           plan.segment[0].sourceRect.x == 512 &&
           plan.segment[0].sourceRect.width == 128 &&
           plan.segment[0].outputRect.x == 0 &&
           plan.segment[0].outputRect.width == 128,
           "Vagrant left display segment maps to output start");
    expect(plan.segment[1].tile == GLES_VRAM_TILE_RIGHT &&
           plan.segment[1].sourceRect.x == 0 &&
           plan.segment[1].sourceRect.width == 384 &&
           plan.segment[1].outputRect.x == 128 &&
           plan.segment[1].outputRect.width == 384,
           "Vagrant right display segment follows left segment");

    rect.x = 896;
    rect.y = 256;
    rect.width = 256;
    rect.height = 240;
    expect(GlesVramBuildDisplayPlan(&rect, &plan),
           "X-wrapped display plan is accepted");
    expect(plan.segmentCount == 2 &&
           plan.segment[0].tile == GLES_VRAM_TILE_RIGHT &&
           plan.segment[0].sourceRect.x == 256 &&
           plan.segment[0].outputRect.x == 0 &&
           plan.segment[0].outputRect.width == 128 &&
           plan.segment[1].tile == GLES_VRAM_TILE_LEFT &&
           plan.segment[1].sourceRect.x == 0 &&
           plan.segment[1].outputRect.x == 128 &&
           plan.segment[1].outputRect.width == 128,
           "X wrap continues from right tile to left tile");

    rect.x = 600;
    rect.y = 500;
    rect.width = 100;
    rect.height = 20;
    expect(GlesVramBuildDisplayPlan(&rect, &plan),
           "X-boundary plus Y-wrap display plan is accepted");
    expect(plan.segmentCount == 4 &&
           plan.segment[0].outputRect.y == 0 &&
           plan.segment[1].outputRect.y == 0 &&
           plan.segment[2].outputRect.y == 12 &&
           plan.segment[3].outputRect.y == 12,
           "Y wrap keeps both tile pieces in adjacent output rows");
}

static void TestDisplayPlanProperties(void)
{
    uint32_t randomState = 0x53445034u;
    int test;

    for (test = 0; test < 10000; test++)
    {
        GlesVramDisplayPlan plan;
        GlesVramRect rect;
        int area = 0;
        int segment;

        rect.x = (int)(NextRandom(&randomState) % 3072u) - 1024;
        rect.y = (int)(NextRandom(&randomState) % 1536u) - 512;
        rect.width = (int)(NextRandom(&randomState) % 1024u) + 1;
        rect.height = (int)(NextRandom(&randomState) % 512u) + 1;
        expect(GlesVramBuildDisplayPlan(&rect, &plan),
               "valid random display rectangle builds a plan");
        for (segment = 0; segment < plan.segmentCount; segment++)
        {
            const GlesVramDisplaySegment *part = &plan.segment[segment];
            int tileWidth = part->tile == GLES_VRAM_TILE_LEFT ?
                            GLES_VRAM_LEFT_WIDTH :
                            GLES_VRAM_RIGHT_WIDTH;
            area += part->sourceRect.width * part->sourceRect.height;
            expect(part->sourceRect.x >= 0 &&
                   part->sourceRect.y >= 0 &&
                   part->sourceRect.x + part->sourceRect.width <= tileWidth &&
                   part->sourceRect.y + part->sourceRect.height <=
                       GLES_VRAM_HEIGHT,
                   "display source stays inside its backing texture");
            expect(part->outputRect.x >= 0 &&
                   part->outputRect.y >= 0 &&
                   part->outputRect.x + part->outputRect.width <= rect.width &&
                   part->outputRect.y + part->outputRect.height <= rect.height,
                   "display segment stays inside logical output");
        }
        expect(area == rect.width * rect.height,
               "display segments preserve the complete display area");
    }
}

static uint32_t NextRandom(uint32_t *state)
{
    *state = *state * 1664525u + 1013904223u;
    return *state;
}

static int Normalize(int value, int limit)
{
    int result = value % limit;
    return result < 0 ? result + limit : result;
}

static void TestSpanProperties(void)
{
    GlesVramTileSpan span[GLES_VRAM_MAX_TILE_SPANS];
    uint32_t randomState = 0x640512u;
    int test;

    for (test = 0; test < 10000; test++)
    {
        GlesVramRect rect;
        int count;
        int area = 0;
        int i;

        rect.x = (int)(NextRandom(&randomState) % 4097u) - 2048;
        rect.y = (int)(NextRandom(&randomState) % 2049u) - 1024;
        rect.width = (int)(NextRandom(&randomState) % 1024u) + 1;
        rect.height = (int)(NextRandom(&randomState) % 512u) + 1;
        count = GlesVramBuildTileSpans(&rect, span);
        expect(count >= 1 && count <= GLES_VRAM_MAX_TILE_SPANS,
               "property span count stays within capacity");

        for (i = 0; i < count; i++)
        {
            int expectedX =
                (Normalize(rect.x, GLES_VRAM_WIDTH) + span[i].commandX) %
                GLES_VRAM_WIDTH;
            int expectedY =
                (Normalize(rect.y, GLES_VRAM_HEIGHT) + span[i].commandY) %
                GLES_VRAM_HEIGHT;

            area += SpanArea(&span[i]);
            expect(span[i].vramRect.x == expectedX &&
                   span[i].vramRect.y == expectedY,
                   "property command offsets map back to VRAM coordinates");
            expect(span[i].vramRect.width > 0 &&
                   span[i].vramRect.height > 0,
                   "property spans are never empty");
            expect(span[i].vramRect.y + span[i].vramRect.height <=
                       GLES_VRAM_HEIGHT,
                   "property span stays within VRAM height");
            if (span[i].tile == GLES_VRAM_TILE_LEFT)
            {
                expect(span[i].localRect.x == span[i].vramRect.x &&
                       span[i].localRect.x + span[i].localRect.width <=
                           GLES_VRAM_LEFT_WIDTH,
                       "property left local mapping is valid");
            }
            else
            {
                expect(span[i].tile == GLES_VRAM_TILE_RIGHT &&
                       span[i].localRect.x ==
                           span[i].vramRect.x - GLES_VRAM_RIGHT_X &&
                       span[i].localRect.x + span[i].localRect.width <=
                           GLES_VRAM_RIGHT_WIDTH,
                       "property right local mapping is valid");
            }
        }
        expect(area == rect.width * rect.height,
               "property tile spans preserve rectangle area");
    }
}

int main(void)
{
    TestTransferSize();
    TestSimpleTileSpans();
    TestBoundarySplit();
    TestWrapSplits();
    TestFullVramFromOffset();
    TestInvalidInput();
    TestDisplayPlans();
    TestDisplayPlanProperties();
    TestSpanProperties();

    if (failures == 0)
        printf("PASS gpu_vram_tiling_rect\n");
    return failures != 0;
}
