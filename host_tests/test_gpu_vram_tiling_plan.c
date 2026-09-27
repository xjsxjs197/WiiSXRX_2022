#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../GlesGpu/gpuVramTiling.h"

static void PutLe32(unsigned char *packet, int word, uint32_t value)
{
    unsigned char *p = packet + word * 4;
    p[0] = (unsigned char)value;
    p[1] = (unsigned char)(value >> 8);
    p[2] = (unsigned char)(value >> 16);
    p[3] = (unsigned char)(value >> 24);
}

static uint32_t Position(int x, int y)
{
    return (uint32_t)(uint16_t)x |
           ((uint32_t)(uint16_t)y << 16);
}

static void TestV8RightPrimitive(void)
{
    unsigned char packet[8 * 4] = {0};
    GlesVramRect drawArea = {0, 0, 1024, 512};
    GlesVramPoint offset = {0, 0};
    GlesVramPrimitivePlan plan;

    PutLe32(packet, 0, 0x38000000u);
    PutLe32(packet, 1, Position(704, 100));
    PutLe32(packet, 3, Position(767, 100));
    PutLe32(packet, 5, Position(704, 180));
    PutLe32(packet, 7, Position(767, 180));
    assert(GlesVramBuildPrimitivePlan(0x38, packet, 8,
                                      &drawArea, &offset, &plan));
    assert(plan.spanCount == 1);
    assert(plan.span[0].tile == GLES_VRAM_TILE_RIGHT);
    assert(plan.span[0].vramRect.x == 704);
    assert(plan.span[0].localRect.x == 64);
    assert(plan.span[0].localRect.width == 64);
}

static void TestCrossBoundaryRectangle(void)
{
    unsigned char packet[3 * 4] = {0};
    GlesVramRect drawArea = {0, 0, 1024, 512};
    GlesVramPoint offset = {0, 0};
    GlesVramPrimitivePlan plan;

    PutLe32(packet, 0, 0x60000000u);
    PutLe32(packet, 1, Position(620, 100));
    PutLe32(packet, 2, Position(40, 20));
    assert(GlesVramBuildPrimitivePlan(0x60, packet, 3,
                                      &drawArea, &offset, &plan));
    assert(plan.spanCount == 2);
    assert(plan.span[0].tile == GLES_VRAM_TILE_LEFT);
    assert(plan.span[0].vramRect.x == 620);
    assert(plan.span[0].vramRect.width == 20);
    assert(plan.span[1].tile == GLES_VRAM_TILE_RIGHT);
    assert(plan.span[1].vramRect.x == 640);
    assert(plan.span[1].vramRect.width == 20);
    assert(plan.span[0].vramRect.x + plan.span[0].vramRect.width ==
           plan.span[1].vramRect.x);
}

static void TestDrawOffsetAndAreaClip(void)
{
    unsigned char packet[4 * 4] = {0};
    GlesVramRect drawArea = {630, 40, 30, 30};
    GlesVramPoint offset = {600, 20};
    GlesVramPrimitivePlan plan;

    PutLe32(packet, 0, 0x20000000u);
    PutLe32(packet, 1, Position(20, 10));
    PutLe32(packet, 2, Position(80, 10));
    PutLe32(packet, 3, Position(20, 80));
    assert(GlesVramBuildPrimitivePlan(0x20, packet, 4,
                                      &drawArea, &offset, &plan));
    assert(plan.writeRect.x == 630);
    assert(plan.writeRect.y == 40);
    assert(plan.writeRect.width == 30);
    assert(plan.writeRect.height == 30);
    assert(plan.spanCount == 2);
    assert(plan.span[0].vramRect.width == 10);
    assert(plan.span[1].vramRect.width == 20);
}

static void TestFillAndWrap(void)
{
    unsigned char packet[3 * 4] = {0};
    GlesVramPrimitivePlan plan;

    PutLe32(packet, 0, 0x02000000u);
    PutLe32(packet, 1, Position(0, 0));
    PutLe32(packet, 2, Position(1023, 511));
    assert(GlesVramBuildPrimitivePlan(0x02, packet, 3,
                                      NULL, NULL, &plan));
    assert(plan.isFill);
    assert(plan.writeRect.width == 1024);
    assert(plan.spanCount == 2);
    assert(plan.span[0].localRect.width == 640);
    assert(plan.span[1].localRect.width == 384);

    memset(packet, 0, sizeof(packet));
    PutLe32(packet, 0, 0x02000000u);
    PutLe32(packet, 1, Position(1008, 500));
    PutLe32(packet, 2, Position(17, 20));
    assert(GlesVramBuildPrimitivePlan(0x02, packet, 3,
                                      NULL, NULL, &plan));
    assert(plan.writeRect.width == 32);
    assert(plan.spanCount == 4);
    assert(plan.span[0].tile == GLES_VRAM_TILE_RIGHT);
    assert(plan.span[0].vramRect.x == 1008);
    assert(plan.span[0].vramRect.y == 500);
    assert(plan.span[1].tile == GLES_VRAM_TILE_LEFT);
    assert(plan.span[1].vramRect.x == 0);
    assert(plan.span[2].vramRect.y == 0);
    assert(plan.span[3].vramRect.x == 0);
}

static void TestSemiTransparentPrimitive(void)
{
    unsigned char packet[3 * 4] = {0};
    GlesVramRect drawArea = {0, 0, 1024, 512};
    GlesVramPoint offset = {0, 0};
    GlesVramPrimitivePlan plan;

    PutLe32(packet, 0, 0x62000000u);
    PutLe32(packet, 1, Position(20, 20));
    PutLe32(packet, 2, Position(16, 16));
    assert(GlesVramBuildPrimitivePlan(0x62, packet, 3,
                                      &drawArea, &offset, &plan));
    assert(plan.spanCount == 1);
    assert(plan.span[0].tile == GLES_VRAM_TILE_LEFT);
    assert(plan.span[0].vramRect.x == 20);
    assert(plan.span[0].vramRect.y == 20);
    assert(plan.span[0].vramRect.width == 16);
    assert(plan.span[0].vramRect.height == 16);

    /* FF7/FF9 alternate 320-pixel display pages.  A semitransparent menu
     * primitive must remain in the physical page selected by DrawOffset,
     * rather than falling back to the legacy shared screen EFB. */
    offset.x = 320;
    assert(GlesVramBuildPrimitivePlan(0x62, packet, 3,
                                      &drawArea, &offset, &plan));
    assert(plan.spanCount == 1);
    assert(plan.span[0].tile == GLES_VRAM_TILE_LEFT);
    assert(plan.span[0].vramRect.x == 340);
    assert(plan.span[0].vramRect.width == 16);
}

static void TestPolyline(void)
{
    unsigned char packet[6 * 4] = {0};
    GlesVramRect drawArea = {0, 0, 1024, 512};
    GlesVramPoint offset = {0, 0};
    GlesVramPrimitivePlan plan;

    PutLe32(packet, 0, 0x48000000u);
    PutLe32(packet, 1, Position(630, 10));
    PutLe32(packet, 2, Position(650, 20));
    PutLe32(packet, 3, Position(660, 30));
    PutLe32(packet, 4, 0x50005000u);
    assert(GlesVramBuildPrimitivePlan(0x48, packet, 6,
                                      &drawArea, &offset, &plan));
    assert(plan.spanCount == 2);
    assert(plan.writeRect.x == 630);
    assert(plan.writeRect.width == 32);
    assert(plan.writeRect.height == 22);
}

static void TestLineExpandedFootprintCrossesBoundary(void)
{
    unsigned char packet[3 * 4] = {0};
    GlesVramRect drawArea = {0, 0, 1024, 512};
    GlesVramPoint offset = {0, 0};
    GlesVramPrimitivePlan plan;

    PutLe32(packet, 0, 0x40000000u);
    PutLe32(packet, 1, Position(630, 100));
    PutLe32(packet, 2, Position(639, 100));
    assert(GlesVramBuildPrimitivePlan(0x40, packet, 3,
                                      &drawArea, &offset, &plan));
    assert(plan.writeRect.x == 630);
    assert(plan.writeRect.width == 11);
    assert(plan.writeRect.height == 2);
    assert(plan.spanCount == 2);
    assert(plan.span[0].tile == GLES_VRAM_TILE_LEFT);
    assert(plan.span[0].vramRect.width == 10);
    assert(plan.span[1].tile == GLES_VRAM_TILE_RIGHT);
    assert(plan.span[1].vramRect.x == 640);
    assert(plan.span[1].vramRect.width == 1);
}

int main(void)
{
    TestV8RightPrimitive();
    TestCrossBoundaryRectangle();
    TestDrawOffsetAndAreaClip();
    TestFillAndWrap();
    TestSemiTransparentPrimitive();
    TestPolyline();
    TestLineExpandedFootprintCrossesBoundary();
    puts("gpuVramTiling primitive plan tests passed");
    return 0;
}
