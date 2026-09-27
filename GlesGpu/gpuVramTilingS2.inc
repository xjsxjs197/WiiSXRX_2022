/* Included by gpuPlugin.c immediately before gpuPrim.c.  The wrappers keep
 * each GP0 handler single-shot and replay only its final GX draw submission. */
#ifdef GLES_VRAM_LR_TILING_S2_EXPERIMENT

typedef void (*GlesVramS2DrawOneFn)(void *vertexAddress);

static void GlesVramS2ReplayOne(GlesVramS2DrawOneFn draw,
                                void *vertexAddress)
{
    int passCount;
    int pass;

    if (!GlesVramTilingS2CommandActive())
    {
        draw(vertexAddress);
        return;
    }

    passCount = GlesVramTilingS2PassCount();
    for (pass = 0; pass < passCount; pass++)
    {
        GlesVramTileSpan span;
        int isFill = 0;
        void *passVertices = vertexAddress;
        OGLVertex fillVertices[4];

        if (!GlesVramTilingS2PreparePass(pass, &span, &isFill))
            continue;

        if (isFill)
        {
            memcpy(fillVertices, vertexAddress, sizeof(fillVertices));
            fillVertices[0].x = (float)span.vramRect.x;
            fillVertices[0].y = (float)span.vramRect.y;
            fillVertices[1].x = (float)(span.vramRect.x +
                                        span.vramRect.width);
            fillVertices[1].y = (float)span.vramRect.y;
            fillVertices[2].x = (float)(span.vramRect.x +
                                        span.vramRect.width);
            fillVertices[2].y = (float)(span.vramRect.y +
                                        span.vramRect.height);
            fillVertices[3].x = (float)span.vramRect.x;
            fillVertices[3].y = (float)(span.vramRect.y +
                                        span.vramRect.height);
            passVertices = fillVertices;
        }

        draw(passVertices);
        GlesVramTilingS2FinishPass();
    }
}

static void GlesVramS2DrawTexturedQuad(void *vertexAddress,
                                      int changePointOrder)
{
    int passCount;
    int pass;

    if (!GlesVramTilingS2CommandActive())
    {
        glPRIMdrawTexturedQuad(vertexAddress, changePointOrder);
        return;
    }

    passCount = GlesVramTilingS2PassCount();
    for (pass = 0; pass < passCount; pass++)
    {
        if (!GlesVramTilingS2PreparePass(pass, NULL, NULL))
            continue;
        glPRIMdrawTexturedQuad(vertexAddress, changePointOrder);
        GlesVramTilingS2FinishPass();
    }
}

#define GLES_VRAM_S2_WRAP_ONE(wrapper, original) \
    static void wrapper(void *vertexAddress) \
    { \
        GlesVramS2ReplayOne(original, vertexAddress); \
    }

GLES_VRAM_S2_WRAP_ONE(GlesVramS2DrawTexturedTri,
                      glPRIMdrawTexturedTri)
GLES_VRAM_S2_WRAP_ONE(GlesVramS2DrawTexGouraudTriColor,
                      glPRIMdrawTexGouraudTriColor)
GLES_VRAM_S2_WRAP_ONE(GlesVramS2DrawTexGouraudTriColorQuad,
                      glPRIMdrawTexGouraudTriColorQuad)
GLES_VRAM_S2_WRAP_ONE(GlesVramS2DrawTri, glPRIMdrawTri)
GLES_VRAM_S2_WRAP_ONE(GlesVramS2DrawTri2, glPRIMdrawTri2)
GLES_VRAM_S2_WRAP_ONE(GlesVramS2DrawGouraudTriColor,
                      glPRIMdrawGouraudTriColor)
GLES_VRAM_S2_WRAP_ONE(GlesVramS2DrawGouraudTri2Color,
                      glPRIMdrawGouraudTri2Color)
GLES_VRAM_S2_WRAP_ONE(GlesVramS2DrawFlatLine, glPRIMdrawFlatLine)
GLES_VRAM_S2_WRAP_ONE(GlesVramS2DrawGouraudLine,
                      glPRIMdrawGouraudLine)
GLES_VRAM_S2_WRAP_ONE(GlesVramS2DrawQuad, glPRIMdrawQuad)

#undef GLES_VRAM_S2_WRAP_ONE
#endif
