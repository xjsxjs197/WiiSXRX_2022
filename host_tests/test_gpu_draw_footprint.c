#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../GlesGpu/gpuDrawFootprint.h"

static int failures;

static void expect(int condition, const char *message)
{
    if (!condition)
    {
        printf("FAIL %s\n", message);
        failures++;
    }
}

static uint32_t Point(int x, int y)
{
    return ((uint32_t)y & 0x7ffu) << 16 |
           ((uint32_t)x & 0x7ffu);
}

static void PutWord(unsigned char *packet, int index, uint32_t value)
{
    packet[index * 4 + 0] = (unsigned char)value;
    packet[index * 4 + 1] = (unsigned char)(value >> 8);
    packet[index * 4 + 2] = (unsigned char)(value >> 16);
    packet[index * 4 + 3] = (unsigned char)(value >> 24);
}

static void NewPacket(unsigned char *packet, unsigned int opcode)
{
    memset(packet, 0, 256 * 4);
    PutWord(packet, 0, (uint32_t)opcode << 24);
}

static GlesGpuDrawFootprintState FullState(void)
{
    GlesGpuDrawFootprintState state;
    state.drawOffsetX = 0;
    state.drawOffsetY = 0;
    state.drawArea.x = 0;
    state.drawArea.y = 0;
    state.drawArea.width = 1024;
    state.drawArea.height = 512;
    return state;
}

static int RectIs(const GlesVramCommandRect *rect,
                  int x, int y, int width, int height)
{
    return rect->x == x && rect->y == y &&
           rect->width == width && rect->height == height;
}

static void TestPolygonLayouts(void)
{
    static const struct
    {
        unsigned int opcode;
        int indexes[4];
        int words;
        int vertices;
    } cases[] = {
        {0x20, {1, 2, 3, 0}, 4, 3},
        {0x24, {1, 3, 5, 0}, 7, 3},
        {0x28, {1, 2, 3, 4}, 5, 4},
        {0x2c, {1, 3, 5, 7}, 9, 4},
        {0x30, {1, 3, 5, 0}, 6, 3},
        {0x34, {1, 4, 7, 0}, 9, 3},
        {0x38, {1, 3, 5, 7}, 8, 4},
        {0x3c, {1, 4, 7, 10}, 12, 4}
    };
    unsigned char packet[256 * 4];
    GlesGpuDrawFootprintState state = FullState();
    GlesGpuDrawFootprint footprint;
    unsigned int index;

    for (index = 0; index < sizeof(cases) / sizeof(cases[0]); index++)
    {
        int vertex;
        NewPacket(packet, cases[index].opcode);
        for (vertex = 0; vertex < cases[index].vertices; vertex++)
        {
            static const int x[4] = {10, 30, 20, 40};
            static const int y[4] = {20, 40, 60, 10};
            PutWord(packet, cases[index].indexes[vertex],
                    Point(x[vertex], y[vertex]));
        }
        expect(GlesGpuPlanDrawFootprint(packet, cases[index].words,
                                        &state, &footprint),
               "polygon opcode is recognized");
        expect(footprint.kind == GLES_GPU_DRAW_FOOTPRINT_POLYGON &&
               footprint.hasPixels,
               "polygon produces a footprint");
        if (cases[index].vertices == 3)
            expect(RectIs(&footprint.rect, 10, 20, 21, 41),
                   "triangle layout decodes all vertices");
        else
            expect(RectIs(&footprint.rect, 10, 10, 31, 51),
                   "quad layout decodes both triangles");
    }
}

static void TestPolygonOffsetAndClipping(void)
{
    unsigned char packet[256 * 4];
    GlesGpuDrawFootprintState state = FullState();
    GlesGpuDrawFootprint footprint;

    NewPacket(packet, 0x20);
    PutWord(packet, 1, Point(5, 5));
    PutWord(packet, 2, Point(30, 5));
    PutWord(packet, 3, Point(5, 30));
    state.drawOffsetX = 10;
    state.drawOffsetY = 20;
    state.drawArea.x = 20;
    state.drawArea.y = 30;
    state.drawArea.width = 11;
    state.drawArea.height = 16;
    expect(GlesGpuPlanDrawFootprint(packet, 4, &state, &footprint) &&
           footprint.hasPixels && footprint.clipped &&
           RectIs(&footprint.unclippedRect, 15, 25, 26, 26) &&
           RectIs(&footprint.rect, 20, 30, 11, 16),
           "DrawOffset applies before half-open DrawArea clipping");

    state.drawArea.x = 100;
    state.drawArea.y = 100;
    state.drawArea.width = 20;
    state.drawArea.height = 20;
    expect(GlesGpuPlanDrawFootprint(packet, 4, &state, &footprint) &&
           !footprint.hasPixels && footprint.whollyOutside,
           "DrawArea can wholly reject a polygon");

    state = FullState();
    PutWord(packet, 1, Point(-1024, 0));
    PutWord(packet, 2, Point(1, 0));
    PutWord(packet, 3, Point(0, 1));
    expect(GlesGpuPlanDrawFootprint(packet, 4, &state, &footprint) &&
           !footprint.hasPixels && footprint.discarded &&
           !footprint.whollyOutside,
           "oversized triangle is culled before clipping");
}

static void TestQuadTriangleCulling(void)
{
    unsigned char packet[256 * 4];
    GlesGpuDrawFootprintState state = FullState();
    GlesGpuDrawFootprint footprint;

    NewPacket(packet, 0x28);
    PutWord(packet, 1, Point(-1024, 10));
    PutWord(packet, 2, Point(1, 10));
    PutWord(packet, 3, Point(2, 11));
    PutWord(packet, 4, Point(3, 12));
    expect(GlesGpuPlanDrawFootprint(packet, 5, &state, &footprint) &&
           footprint.hasPixels && !footprint.discarded &&
           RectIs(&footprint.rect, 1, 10, 3, 3),
           "quad retains its valid triangle when the first is oversized");
}

static void TestLines(void)
{
    unsigned char packet[256 * 4];
    GlesGpuDrawFootprintState state = FullState();
    GlesGpuDrawFootprint footprint;

    NewPacket(packet, 0x40);
    PutWord(packet, 1, Point(5, 6));
    PutWord(packet, 2, Point(10, 8));
    expect(GlesGpuPlanDrawFootprint(packet, 3, &state, &footprint) &&
           footprint.kind == GLES_GPU_DRAW_FOOTPRINT_LINE &&
           footprint.hasPixels && RectIs(&footprint.rect, 5, 6, 7, 4),
           "flat line includes offsetline maximum-edge expansion");

    NewPacket(packet, 0x50);
    PutWord(packet, 1, Point(20, 30));
    PutWord(packet, 2, 0x00112233u);
    PutWord(packet, 3, Point(25, 35));
    expect(GlesGpuPlanDrawFootprint(packet, 4, &state, &footprint) &&
           RectIs(&footprint.rect, 20, 30, 7, 7),
           "Gouraud line skips its second color word");

    NewPacket(packet, 0x48);
    PutWord(packet, 1, Point(1, 1));
    PutWord(packet, 2, Point(4, 1));
    PutWord(packet, 3, Point(4, 5));
    PutWord(packet, 4, 0x50005000u);
    expect(GlesGpuPlanDrawFootprint(packet, 5, &state, &footprint) &&
           footprint.hasPixels && RectIs(&footprint.rect, 1, 1, 5, 6),
           "flat polyline unions every segment before terminator");

    NewPacket(packet, 0x58);
    PutWord(packet, 1, Point(10, 10));
    PutWord(packet, 2, 0x00010203u);
    PutWord(packet, 3, Point(15, 10));
    PutWord(packet, 4, 0x00040506u);
    PutWord(packet, 5, Point(15, 15));
    PutWord(packet, 6, 0x50005000u);
    expect(GlesGpuPlanDrawFootprint(packet, 7, &state, &footprint) &&
           footprint.hasPixels && RectIs(&footprint.rect, 10, 10, 7, 7),
           "Gouraud polyline distinguishes color and terminator words");

    NewPacket(packet, 0x48);
    PutWord(packet, 1, Point(1, 1));
    PutWord(packet, 2, Point(2, 2));
    expect(GlesGpuPlanDrawFootprint(packet, 3, &state, &footprint) &&
           footprint.malformed && !footprint.hasPixels,
           "unterminated polyline is marked malformed");
}

static void TestRectangles(void)
{
    unsigned char packet[256 * 4];
    GlesGpuDrawFootprintState state = FullState();
    GlesGpuDrawFootprint footprint;

    NewPacket(packet, 0x60);
    PutWord(packet, 1, Point(100, 200));
    PutWord(packet, 2, 20u | (30u << 16));
    expect(GlesGpuPlanDrawFootprint(packet, 3, &state, &footprint) &&
           footprint.kind == GLES_GPU_DRAW_FOOTPRINT_RECTANGLE &&
           RectIs(&footprint.rect, 100, 200, 20, 30),
           "variable flat rectangle uses word 2 size");

    NewPacket(packet, 0x64);
    PutWord(packet, 1, Point(100, 200));
    PutWord(packet, 2, 0x12345678u);
    PutWord(packet, 3, 21u | (31u << 16));
    expect(GlesGpuPlanDrawFootprint(packet, 4, &state, &footprint) &&
           RectIs(&footprint.rect, 100, 200, 21, 31),
           "variable textured rectangle uses word 3 size");

    NewPacket(packet, 0x68);
    PutWord(packet, 1, Point(10, 20));
    expect(GlesGpuPlanDrawFootprint(packet, 2, &state, &footprint) &&
           RectIs(&footprint.rect, 10, 20, 1, 1),
           "1x1 rectangle footprint");

    NewPacket(packet, 0x74);
    PutWord(packet, 1, Point(10, 20));
    PutWord(packet, 2, 0);
    expect(GlesGpuPlanDrawFootprint(packet, 3, &state, &footprint) &&
           RectIs(&footprint.rect, 10, 20, 8, 8),
           "textured 8x8 rectangle footprint");

    NewPacket(packet, 0x78);
    PutWord(packet, 1, Point(10, 20));
    expect(GlesGpuPlanDrawFootprint(packet, 2, &state, &footprint) &&
           RectIs(&footprint.rect, 10, 20, 16, 16),
           "16x16 rectangle footprint");

    NewPacket(packet, 0x60);
    PutWord(packet, 1, Point(1020, 20));
    PutWord(packet, 2, 10u | (2u << 16));
    state.drawOffsetX = 10;
    expect(GlesGpuPlanDrawFootprint(packet, 3, &state, &footprint) &&
           footprint.whollyOutside &&
           RectIs(&footprint.unclippedRect, -1018, 20, 10, 2),
           "rectangle position truncates to signed 11 bits after offset");

    state = FullState();
    NewPacket(packet, 0x60);
    PutWord(packet, 1, Point(10, 20));
    PutWord(packet, 2, 0u | (2u << 16));
    expect(GlesGpuPlanDrawFootprint(packet, 3, &state, &footprint) &&
           !footprint.hasPixels && footprint.discarded &&
           !footprint.whollyOutside,
           "zero-width rectangle is empty");

    state.drawArea.x = 1023;
    state.drawArea.y = 511;
    state.drawArea.width = 1;
    state.drawArea.height = 1;
    NewPacket(packet, 0x68);
    PutWord(packet, 1, Point(1023, 511));
    expect(GlesGpuPlanDrawFootprint(packet, 2, &state, &footprint) &&
           footprint.hasPixels &&
           RectIs(&footprint.rect, 1023, 511, 1, 1),
           "DrawArea bottom-right endpoint is inclusive");
}

static void TestRecognitionAndEndian(void)
{
    unsigned char packet[256 * 4];
    GlesGpuDrawFootprintState state = FullState();
    GlesGpuDrawFootprint footprint;

    NewPacket(packet, 0x02);
    expect(!GlesGpuPlanDrawFootprint(packet, 3, &state, &footprint),
           "non-Draw command is not recognized");

    NewPacket(packet, 0x20);
    PutWord(packet, 1, Point(1, 2));
    PutWord(packet, 2, Point(3, 4));
    PutWord(packet, 3, Point(5, 6));
    expect(packet[3] == 0x20 &&
           GlesGpuPlanDrawFootprint(packet, 4, &state, &footprint) &&
           RectIs(&footprint.rect, 1, 2, 5, 5),
           "packet decoding is explicitly little endian");

    /* The pure planner describes the nominal command encoding.  Runtime
     * dispatch separately suppresses this footprint because primTableJGx
     * maps 6Ch..6Fh to primNI. */
    NewPacket(packet, 0x6c);
    PutWord(packet, 1, Point(7, 8));
    expect(GlesGpuPlanDrawFootprint(packet, 2, &state, &footprint) &&
           footprint.kind == GLES_GPU_DRAW_FOOTPRINT_RECTANGLE &&
           footprint.hasPixels && RectIs(&footprint.rect, 7, 8, 1, 1),
           "planner keeps nominal 6Ch rectangle semantics");
}

int main(void)
{
    TestPolygonLayouts();
    TestPolygonOffsetAndClipping();
    TestQuadTriangleCulling();
    TestLines();
    TestRectangles();
    TestRecognitionAndEndian();

    if (failures == 0)
        printf("PASS gpu_draw_footprint\n");
    return failures != 0;
}
