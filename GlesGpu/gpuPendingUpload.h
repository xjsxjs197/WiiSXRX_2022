#ifndef GPU_PENDING_UPLOAD_H
#define GPU_PENDING_UPLOAD_H

#include <stdint.h>

#include "gpuVramCommandRect.h"

#define GLES_GPU_PENDING_UPLOAD_CAPACITY 32

typedef struct GlesGpuPendingUploadItemTag
{
    GlesVramCommandRect rect;
    uint32_t order;
} GlesGpuPendingUploadItem;

typedef struct GlesGpuPendingUploadSetTag
{
    GlesGpuPendingUploadItem item[GLES_GPU_PENDING_UPLOAD_CAPACITY];
    int count;
    uint32_t nextOrder;
} GlesGpuPendingUploadSet;

void GlesGpuPendingUploadReset(GlesGpuPendingUploadSet *set);

/* Adds a possibly wrapped VRAM rectangle transactionally. */
int GlesGpuPendingUploadAddWrapped(GlesGpuPendingUploadSet *set,
                                   const GlesVramCommandRect *rect);

/* Returns the oldest item intersecting query. */
int GlesGpuPendingUploadFindOldest(
    const GlesGpuPendingUploadSet *set,
    const GlesVramCommandRect *query,
    int *itemIndex,
    GlesVramCommandRect *intersection);

/* Removes a physical, non-wrapped rectangle transactionally. */
int GlesGpuPendingUploadSubtract(GlesGpuPendingUploadSet *set,
                                 const GlesVramCommandRect *rect);

#endif
