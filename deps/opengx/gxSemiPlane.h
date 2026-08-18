/***************************************************************************
                          gxSemiPlane.h
                             -------------------
    Semi-transparent texture-plane update decision used by OpenGX.
 ***************************************************************************/

#ifndef GX_SEMI_PLANE_H
#define GX_SEMI_PLANE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum GxSemiPlaneAction
{
    GX_SEMI_ACTION_NONE = 0,
    GX_SEMI_ACTION_COPY,
    GX_SEMI_ACTION_ALLOC_COPY
} GxSemiPlaneAction;

/*
 * Decide what glTexImage2D() must do for the semi-transparent plane.
 * The first semi upload allocates and copies; same-size reuse only copies;
 * an upload without semi texels does not touch the retained plane.
 */
static inline GxSemiPlaneAction GxSemiPlaneUpdateAction(int hasSemiData,
                                                        int newHasSemi)
{
    if (!newHasSemi)
        return GX_SEMI_ACTION_NONE;
    return hasSemiData ? GX_SEMI_ACTION_COPY : GX_SEMI_ACTION_ALLOC_COPY;
}

#ifdef __cplusplus
}
#endif

#endif /* GX_SEMI_PLANE_H */
