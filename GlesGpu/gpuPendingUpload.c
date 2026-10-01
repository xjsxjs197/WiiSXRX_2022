#include "gpuPendingUpload.h"

#include <stddef.h>
#include <string.h>

static int GlesGpuPendingUploadRectInsideVram(
    const GlesVramCommandRect *rect)
{
    return rect != NULL && rect->width > 0 && rect->height > 0 &&
           rect->x >= 0 && rect->y >= 0 &&
           rect->x + rect->width <= GLES_VRAM_COMMAND_WIDTH &&
           rect->y + rect->height <= GLES_VRAM_COMMAND_HEIGHT;
}

static int GlesGpuPendingUploadIntersectionArea(
    const GlesVramCommandRect *first,
    const GlesVramCommandRect *second)
{
    GlesVramCommandRect intersection;

    if (!GlesVramCommandIntersectRects(first, second, &intersection))
        return 0;
    return intersection.width * intersection.height;
}

static int GlesGpuPendingUploadCanMerge(
    const GlesVramCommandRect *first,
    const GlesVramCommandRect *second,
    GlesVramCommandRect *merged)
{
    int x0 = first->x < second->x ? first->x : second->x;
    int y0 = first->y < second->y ? first->y : second->y;
    int x1 = first->x + first->width > second->x + second->width ?
             first->x + first->width : second->x + second->width;
    int y1 = first->y + first->height > second->y + second->height ?
             first->y + first->height : second->y + second->height;
    int unionArea = first->width * first->height +
                    second->width * second->height -
                    GlesGpuPendingUploadIntersectionArea(first, second);

    if ((x1 - x0) * (y1 - y0) != unionArea)
        return 0;
    merged->x = x0;
    merged->y = y0;
    merged->width = x1 - x0;
    merged->height = y1 - y0;
    return 1;
}

static void GlesGpuPendingUploadMergeItems(GlesGpuPendingUploadSet *set)
{
    int first;

    for (first = 0; first < set->count; first++)
    {
        int second = first + 1;
        while (second < set->count)
        {
            GlesVramCommandRect merged;
            if (GlesGpuPendingUploadCanMerge(&set->item[first].rect,
                                             &set->item[second].rect,
                                             &merged))
            {
                set->item[first].rect = merged;
                if (set->item[second].order < set->item[first].order)
                    set->item[first].order = set->item[second].order;
                set->item[second] = set->item[--set->count];
                second = first + 1;
            }
            else
                second++;
        }
    }
}

static int GlesGpuPendingUploadAddPhysical(
    GlesGpuPendingUploadSet *set,
    const GlesVramCommandRect *rect,
    uint32_t order)
{
    int index;

    if (!GlesGpuPendingUploadRectInsideVram(rect))
        return 0;
    for (index = 0; index < set->count; index++)
    {
        GlesVramCommandRect merged;
        if (!GlesGpuPendingUploadCanMerge(&set->item[index].rect,
                                          rect, &merged))
            continue;
        set->item[index].rect = merged;
        if (order < set->item[index].order)
            set->item[index].order = order;
        GlesGpuPendingUploadMergeItems(set);
        return 1;
    }
    if (set->count >= GLES_GPU_PENDING_UPLOAD_CAPACITY)
        return 0;
    set->item[set->count].rect = *rect;
    set->item[set->count].order = order;
    set->count++;
    return 1;
}

void GlesGpuPendingUploadReset(GlesGpuPendingUploadSet *set)
{
    if (set == NULL)
        return;
    memset(set, 0, sizeof(*set));
    set->nextOrder = 1;
}

int GlesGpuPendingUploadAddWrapped(GlesGpuPendingUploadSet *set,
                                   const GlesVramCommandRect *rect)
{
    GlesGpuPendingUploadSet pending;
    GlesVramCommandRectPiece piece[GLES_VRAM_COMMAND_MAX_WRAP_PIECES];
    uint32_t order;
    int count;
    int index;

    if (set == NULL)
        return 0;
    count = GlesVramCommandSplitWrappedRect(rect, piece);
    if (count <= 0)
        return 0;
    pending = *set;
    order = pending.nextOrder++;
    if (pending.nextOrder == 0)
        pending.nextOrder = 1;
    for (index = 0; index < count; index++)
    {
        if (!GlesGpuPendingUploadAddPhysical(
                &pending, &piece[index].rect, order))
            return 0;
    }
    *set = pending;
    return 1;
}

int GlesGpuPendingUploadFindOldest(
    const GlesGpuPendingUploadSet *set,
    const GlesVramCommandRect *query,
    int *itemIndex,
    GlesVramCommandRect *intersection)
{
    uint32_t oldest = 0;
    int found = -1;
    int index;

    if (set == NULL || query == NULL)
        return 0;
    for (index = 0; index < set->count; index++)
    {
        if (!GlesVramCommandRectsOverlap(&set->item[index].rect, query))
            continue;
        if (found < 0 || set->item[index].order < oldest)
        {
            found = index;
            oldest = set->item[index].order;
        }
    }
    if (found < 0)
        return 0;
    if (itemIndex != NULL)
        *itemIndex = found;
    if (intersection != NULL)
        GlesVramCommandIntersectRects(&set->item[found].rect,
                                      query, intersection);
    return 1;
}

static int GlesGpuPendingUploadEmit(
    GlesGpuPendingUploadSet *set,
    const GlesVramCommandRect *rect,
    uint32_t order)
{
    if (rect->width <= 0 || rect->height <= 0)
        return 1;
    if (set->count >= GLES_GPU_PENDING_UPLOAD_CAPACITY)
        return 0;
    set->item[set->count].rect = *rect;
    set->item[set->count].order = order;
    set->count++;
    return 1;
}

int GlesGpuPendingUploadSubtract(GlesGpuPendingUploadSet *set,
                                 const GlesVramCommandRect *rect)
{
    GlesGpuPendingUploadSet result;
    int index;

    if (set == NULL || !GlesGpuPendingUploadRectInsideVram(rect))
        return 0;
    result = *set;
    result.count = 0;
    for (index = 0; index < set->count; index++)
    {
        const GlesGpuPendingUploadItem *item = &set->item[index];
        GlesVramCommandRect cut;
        GlesVramCommandRect part;
        int middleY0;
        int middleY1;

        if (!GlesVramCommandIntersectRects(&item->rect, rect, &cut))
        {
            if (!GlesGpuPendingUploadEmit(
                    &result, &item->rect, item->order))
                return 0;
            continue;
        }

        part.x = item->rect.x;
        part.y = item->rect.y;
        part.width = item->rect.width;
        part.height = cut.y - item->rect.y;
        if (!GlesGpuPendingUploadEmit(&result, &part, item->order))
            return 0;

        part.y = cut.y + cut.height;
        part.height = item->rect.y + item->rect.height - part.y;
        if (!GlesGpuPendingUploadEmit(&result, &part, item->order))
            return 0;

        middleY0 = cut.y > item->rect.y ? cut.y : item->rect.y;
        middleY1 = cut.y + cut.height < item->rect.y + item->rect.height ?
                   cut.y + cut.height : item->rect.y + item->rect.height;
        part.x = item->rect.x;
        part.y = middleY0;
        part.width = cut.x - item->rect.x;
        part.height = middleY1 - middleY0;
        if (!GlesGpuPendingUploadEmit(&result, &part, item->order))
            return 0;

        part.x = cut.x + cut.width;
        part.width = item->rect.x + item->rect.width - part.x;
        if (!GlesGpuPendingUploadEmit(&result, &part, item->order))
            return 0;
    }
    GlesGpuPendingUploadMergeItems(&result);
    *set = result;
    return 1;
}
