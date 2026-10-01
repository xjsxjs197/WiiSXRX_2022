#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../GlesGpu/gpuPendingUpload.h"

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

static int RectIs(const GlesVramCommandRect *rect,
                  int x, int y, int width, int height)
{
    return rect->x == x && rect->y == y &&
           rect->width == width && rect->height == height;
}

static int TotalArea(const GlesGpuPendingUploadSet *set)
{
    int area = 0;
    int index;
    for (index = 0; index < set->count; index++)
        area += set->item[index].rect.width * set->item[index].rect.height;
    return area;
}

static void TestMergeWithoutGap(void)
{
    GlesGpuPendingUploadSet set;
    GlesVramCommandRect rect = {10, 20, 30, 5};

    GlesGpuPendingUploadReset(&set);
    expect(GlesGpuPendingUploadAddWrapped(&set, &rect),
           "first pending rectangle is accepted");
    rect.x = 40;
    rect.width = 10;
    expect(GlesGpuPendingUploadAddWrapped(&set, &rect) && set.count == 1 &&
           RectIs(&set.item[0].rect, 10, 20, 40, 5),
           "edge-adjacent rectangles merge without adding a gap");

    rect.x = 45;
    rect.y = 24;
    rect.width = 10;
    rect.height = 10;
    expect(GlesGpuPendingUploadAddWrapped(&set, &rect) && set.count == 2,
           "L-shaped union remains separate");
}

static void TestWrappedAddAndOldest(void)
{
    GlesGpuPendingUploadSet set;
    GlesVramCommandRect first = {1000, 500, 40, 20};
    GlesVramCommandRect second = {100, 100, 10, 10};
    GlesVramCommandRect query = {0, 0, 1024, 512};
    GlesVramCommandRect intersection;
    int itemIndex = -1;

    GlesGpuPendingUploadReset(&set);
    expect(GlesGpuPendingUploadAddWrapped(&set, &first) && set.count == 4,
           "dual-axis wrap is stored as four physical rectangles");
    expect(GlesGpuPendingUploadAddWrapped(&set, &second),
           "later rectangle is accepted");
    expect(GlesGpuPendingUploadFindOldest(
               &set, &query, &itemIndex, &intersection) &&
           set.item[itemIndex].order == 1,
           "oldest intersecting write is returned first");
}

static void TestSubtract(void)
{
    GlesGpuPendingUploadSet set;
    GlesVramCommandRect rect = {10, 10, 20, 20};
    GlesVramCommandRect cut = {15, 15, 10, 10};
    GlesVramCommandRect query = {15, 15, 10, 10};

    GlesGpuPendingUploadReset(&set);
    GlesGpuPendingUploadAddWrapped(&set, &rect);
    expect(GlesGpuPendingUploadSubtract(&set, &cut) &&
           set.count == 4 && TotalArea(&set) == 300,
           "center subtraction preserves four exact remainder strips");
    expect(!GlesGpuPendingUploadFindOldest(
               &set, &query, NULL, NULL),
           "subtracted area is no longer pending");

    cut.x = 0;
    cut.y = 0;
    cut.width = 1024;
    cut.height = 512;
    expect(GlesGpuPendingUploadSubtract(&set, &cut) && set.count == 0,
           "full VRAM subtraction clears the set");
}

static void TestCapacityIsTransactional(void)
{
    GlesGpuPendingUploadSet set;
    GlesGpuPendingUploadSet before;
    GlesVramCommandRect rect = {0, 0, 1, 1};
    int index;

    GlesGpuPendingUploadReset(&set);
    for (index = 0; index < GLES_GPU_PENDING_UPLOAD_CAPACITY; index++)
    {
        rect.x = index * 2;
        expect(GlesGpuPendingUploadAddWrapped(&set, &rect),
               "capacity setup rectangle is accepted");
    }
    before = set;
    rect.x = 100;
    expect(!GlesGpuPendingUploadAddWrapped(&set, &rect) &&
           set.count == before.count &&
           set.nextOrder == before.nextOrder,
           "capacity failure leaves the pending set unchanged");
}

static void BuildCoverage(const GlesGpuPendingUploadSet *set,
                          unsigned char *coverage)
{
    int index;

    memset(coverage, 0, 1024u * 512u);
    for (index = 0; index < set->count; index++)
    {
        const GlesVramCommandRect *rect = &set->item[index].rect;
        int y;
        for (y = rect->y; y < rect->y + rect->height; y++)
            memset(coverage + y * 1024 + rect->x, 1,
                   (size_t)rect->width);
    }
}

static void MarkWrapped(unsigned char *coverage,
                        const GlesVramCommandRect *rect, int value)
{
    int y;
    int x;
    for (y = 0; y < rect->height; y++)
        for (x = 0; x < rect->width; x++)
            coverage[((rect->y + y) & 511) * 1024 +
                     ((rect->x + x) & 1023)] = (unsigned char)value;
}

static void TestRandomCoverage(void)
{
    const size_t bytes = 1024u * 512u;
    unsigned char *expected = (unsigned char *)calloc(bytes, 1);
    unsigned char *actual = (unsigned char *)calloc(bytes, 1);
    GlesGpuPendingUploadSet set;
    uint32_t random = 0x5e7c0deu;
    int iteration;

    expect(expected != NULL && actual != NULL,
           "random coverage buffers allocate");
    if (expected == NULL || actual == NULL)
    {
        free(expected);
        free(actual);
        return;
    }

    GlesGpuPendingUploadReset(&set);
    for (iteration = 0; iteration < 120; iteration++)
    {
        GlesVramCommandRect rect;
        GlesGpuPendingUploadSet before = set;
        int success;

        rect.x = (int)(NextRandom(&random) & 1023u);
        rect.y = (int)(NextRandom(&random) & 511u);
        rect.width = (int)(NextRandom(&random) % 40u) + 1;
        rect.height = (int)(NextRandom(&random) % 20u) + 1;
        if ((iteration % 3) != 2)
        {
            success = GlesGpuPendingUploadAddWrapped(&set, &rect);
            if (success)
                MarkWrapped(expected, &rect, 1);
            else
                expect(memcmp(&set, &before, sizeof(set)) == 0,
                       "failed random add is transactional");
        }
        else
        {
            if (rect.x + rect.width > 1024)
                rect.width = 1024 - rect.x;
            if (rect.y + rect.height > 512)
                rect.height = 512 - rect.y;
            success = GlesGpuPendingUploadSubtract(&set, &rect);
            if (success)
                MarkWrapped(expected, &rect, 0);
            else
                expect(memcmp(&set, &before, sizeof(set)) == 0,
                       "failed random subtract is transactional");
        }

        BuildCoverage(&set, actual);
        if (memcmp(actual, expected, bytes) != 0)
        {
            expect(0, "random pending coverage remains exact");
            break;
        }
    }
    if (iteration == 120)
        expect(1, "random pending coverage remains exact");
    free(expected);
    free(actual);
}

int main(void)
{
    TestMergeWithoutGap();
    TestWrappedAddAndOldest();
    TestSubtract();
    TestCapacityIsTransactional();
    TestRandomCoverage();

    if (failures == 0)
        printf("PASS gpu_pending_upload\n");
    return failures != 0;
}
