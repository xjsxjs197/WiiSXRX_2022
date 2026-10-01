#ifndef GPU_DRAW_FOOTPRINT_H
#define GPU_DRAW_FOOTPRINT_H

#include <stdint.h>

#include "gpuVramCommandRect.h"

typedef enum GlesGpuDrawFootprintKindTag
{
    GLES_GPU_DRAW_FOOTPRINT_NONE = 0,
    GLES_GPU_DRAW_FOOTPRINT_POLYGON,
    GLES_GPU_DRAW_FOOTPRINT_LINE,
    GLES_GPU_DRAW_FOOTPRINT_RECTANGLE
} GlesGpuDrawFootprintKind;

typedef struct GlesGpuDrawFootprintStateTag
{
    int drawOffsetX;
    int drawOffsetY;
    /* Drawing area uses a half-open rectangle. */
    GlesVramCommandRect drawArea;
} GlesGpuDrawFootprintState;

typedef struct GlesGpuDrawFootprintTag
{
    GlesGpuDrawFootprintKind kind;
    GlesVramCommandRect unclippedRect;
    GlesVramCommandRect rect;
    int hasPixels;
    int clipped;
    int whollyOutside;
    /* Recognized, well-formed command which the PS1 discards before clip. */
    int discarded;
    int malformed;
} GlesGpuDrawFootprint;

/* packet points at little-endian GP0 words, as stored in gpuDataM. */
int GlesGpuPlanDrawFootprint(const void *packet, int wordCount,
                             const GlesGpuDrawFootprintState *state,
                             GlesGpuDrawFootprint *footprint);

#endif
