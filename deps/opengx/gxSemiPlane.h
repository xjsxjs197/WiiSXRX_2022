/***************************************************************************
                          gxSemiPlane.h
                             -------------------
    Semi-transparent texture-plane update orchestration shared by OpenGX
    and host-side tests.
 ***************************************************************************/

#ifndef GX_SEMI_PLANE_H
#define GX_SEMI_PLANE_H

#include <string.h>

#include "gxTextureScramble.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum GxSemiPlaneAction
{
    GX_SEMI_ACTION_NONE = 0,
    GX_SEMI_ACTION_COPY,
    GX_SEMI_ACTION_ALLOC_ZERO_COPY
} GxSemiPlaneAction;

typedef struct GxSemiPlaneState
{
    void *data;        /* NULL when no semi plane is retained */
    unsigned int size; /* tiled byte size of the retained plane */
} GxSemiPlaneState;

/*
 * Decide what a full-image upload must do.  The first semi upload allocates
 * and zero-initializes; same-size reuse only copies into the retained plane;
 * an upload without semi texels does not touch the retained plane.  A size
 * change also requires a fresh allocation and GXTexObj re-initialization.
 */
static inline GxSemiPlaneAction GxSemiPlanePrepareFull(
    const GxSemiPlaneState *retained, int hasSemi,
    unsigned int width, unsigned int height, int *reinitTexObj)
{
    unsigned int tiledSize = GxRgb5a3TiledSize(width, height);

    *reinitTexObj = 0;
    if (!hasSemi)
        return GX_SEMI_ACTION_NONE;
    if (retained->data != NULL && retained->size == tiledSize)
        return GX_SEMI_ACTION_COPY;

    *reinitTexObj = 1;
    return GX_SEMI_ACTION_ALLOC_ZERO_COPY;
}

/*
 * Decide what a sub-image upload must do.  A retained plane is written
 * directly (opaque texels clear old semi texels).  The first semi-touching
 * partial upload allocates and zero-initializes so regions outside the
 * sub-rect are never inherited from unrelated buffer contents.
 */
static inline GxSemiPlaneAction GxSemiPlanePrepareSub(
    const GxSemiPlaneState *retained, int hasSemi, int *reinitTexObj)
{
    *reinitTexObj = 0;
    if (retained->data != NULL)
        return GX_SEMI_ACTION_COPY;
    if (!hasSemi)
        return GX_SEMI_ACTION_NONE;

    *reinitTexObj = 1;
    return GX_SEMI_ACTION_ALLOC_ZERO_COPY;
}

/*
 * Zero the full tiled semi plane.  Must be called after the first allocation
 * and before the first partial write.
 */
static inline void GxSemiPlaneZero(void *plane, unsigned int width,
                                   unsigned int height)
{
    memset(plane, 0, GxRgb5a3TiledSize(width, height));
}

#ifdef __cplusplus
}
#endif

#endif /* GX_SEMI_PLANE_H */
