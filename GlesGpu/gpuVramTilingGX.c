#include "gpuVramTiling.h"

#ifdef GLES_VRAM_LR_TILING_EXPERIMENT

#include <gccore.h>
#include <string.h>
#include "../deps/opengx/GL/gl.h"

#include "gpuVramTilingInternal.h"
#include "../mem2_manager.h"

typedef struct GlesVramBackingTag
{
    void *pixels;
    uint32_t size;
    int width;
    GXTexObj texture;
} GlesVramBacking;

typedef struct GlesVramTilingStateTag
{
    GlesVramBacking backing[2];
    void *staging;
    uint32_t stagingSize;
    GlesVramManagerState manager;
#ifdef GLES_VRAM_LR_TILING_S7_EXPERIMENT
    GlesVramFrontPresentationState frontPresentation;
#ifdef GLES_VRAM_LR_TILING_S7_ANIM_DIAG
    GlesVramS7AnimStats s7AnimStats;
#ifdef GLES_VRAM_LR_TILING_S7_PIPELINE_DIAG
    GlesVramBacking diagEfb;
    uint32_t diagPresentFrame;
    uint32_t diagXfbFrame;
    void *diagXfbPixels;
    int diagXfbWidth;
    int diagXfbHeight;
    int diagXfbStridePixels;
    int diagXfbInFlight;
    int diagXfbReady;
#endif
#endif
#endif
    int initialized;
    int gpuCommandsIssued;
    int stagingInFlight;
    unsigned char backingInFlight[2];
#ifdef GLES_VRAM_LR_TILING_S5_EXPERIMENT
    uint16_t *moveScratch;
    size_t moveScratchPixels;
    uint32_t s5MoveCount;
    uint32_t s5C0Count;
    uint32_t s5ResolveCalls;
    uint32_t s5ResolveWaits;
    uint32_t s5ResolvedBlocks;
#endif
#ifdef GLES_VRAM_LR_TILING_S1_TEST
    int visualTestReady;
#endif
#ifdef GLES_VRAM_LR_TILING_S2_EXPERIMENT
    GlesVramPrimitivePlan s2Plan;
    uint64_t s2Sequence;
    int s2Active;
    int s2PassIndex;
    int s2PassSubmitted;
#ifdef GLES_VRAM_LR_TILING_S7_EXPERIMENT
    GlesVramRect s2FrontDisplay;
    int s2FrontDisplayValid;
#endif
#ifdef GLES_VRAM_LR_TILING_S2_TEST
    int s2VisualTestReady;
#endif
#ifdef GLES_VRAM_LR_TILING_S3_TEST
    int s3VisualTestReady;
    int s3DiagnosticsReady;
    uint16_t s3CpuSamples[4];
    uint16_t s3StagingSamples[4];
    uint16_t s3EfbSamples[4];
    uint16_t s3BackingSamples[4];
#endif
#ifdef GLES_VRAM_LR_TILING_S4_TEST
    int s4VisualTestReady;
#endif
#ifdef GLES_VRAM_LR_TILING_S3_EXPERIMENT
    int s3CpuWritePrepared;
#endif
#endif
} GlesVramTilingState;

static GlesVramTilingState g_vramTiling;

#ifdef GLES_VRAM_LR_TILING_S7_ANIM_DIAG
static uint32_t GlesVramS7AnimHashPixel(uint32_t hash, uint16_t pixel)
{
    hash ^= (uint32_t)pixel;
    return hash * 16777619u;
}

static void GlesVramS7AnimMergeBounds(GlesVramRect *bounds,
                                      uint32_t priorCount,
                                      int x, int y, int width, int height)
{
    int x1 = x + width;
    int y1 = y + height;
    int oldX1;
    int oldY1;

    if (width <= 0 || height <= 0)
        return;
    if (priorCount == 0 || bounds->width <= 0 || bounds->height <= 0)
    {
        bounds->x = x;
        bounds->y = y;
        bounds->width = width;
        bounds->height = height;
        return;
    }
    oldX1 = bounds->x + bounds->width;
    oldY1 = bounds->y + bounds->height;
    if (x < bounds->x) bounds->x = x;
    if (y < bounds->y) bounds->y = y;
    if (x1 > oldX1) oldX1 = x1;
    if (y1 > oldY1) oldY1 = y1;
    bounds->width = oldX1 - bounds->x;
    bounds->height = oldY1 - bounds->y;
}
#endif

/* gpuPlugin owns the normal XFB copy setup.  GX texture copies reuse those
 * registers, so every save restores them before returning to presentation. */
extern void RestoreDispCopyInfo(void);
/* OpenGX owns the TMEM cache-region layout.  Direct GX draws must use the
 * same explicit region path as OpenGX instead of going through its fallback
 * callback (which always returns region 0 without tracking that use). */
extern GXTexRegion texCacheRegionS[8];
#ifdef GLES_VRAM_LR_TILING_S3_EXPERIMENT
extern unsigned short *psxVuw;
#endif

/* OpenGX deliberately keeps PS1 BGR555 channel positions in its RGB5A3
 * textures and uses SWAP0 (B,G,R,A) to correct them in TEV.  Tile backings,
 * however, are GX_CopyTex output and therefore contain ordinary GX RGB5A3.
 * Direct tile-manager draws must not inherit that BGR correction or every
 * restore/upload/present would exchange red and blue once more.
 *
 * Use a private identity table for each immediate direct-GX draw, then put
 * stage 0 back on SWAP0 before OpenGX submits its next primitive.  Re-select
 * the table for every draw because menu/debug rendering may change GX state. */
static void GlesVramBeginNativeColorChannels(void)
{
    GX_SetTevSwapModeTable(GX_TEV_SWAP1, GX_CH_RED, GX_CH_GREEN,
                           GX_CH_BLUE, GX_CH_ALPHA);
    GX_SetTevSwapMode(GX_TEVSTAGE0, GX_TEV_SWAP1, GX_TEV_SWAP1);
}

static void GlesVramEndNativeColorChannels(void)
{
    GX_SetTevSwapModeTable(GX_TEV_SWAP0, GX_CH_BLUE, GX_CH_GREEN,
                           GX_CH_RED, GX_CH_ALPHA);
    GX_SetTevSwapMode(GX_TEVSTAGE0, GX_TEV_SWAP0, GX_TEV_SWAP0);
}

static int GlesVramAllocateBacking(GlesVramBacking *backing, int width)
{
    uint32_t size = GX_GetTexBufferSize((uint16_t)width,
                                        GLES_VRAM_HEIGHT,
                                        GX_TF_RGB5A3, 0, GX_FALSE);

    backing->pixels = _mem2_memalign(32, size);
    if (backing->pixels == NULL)
        return 0;

    backing->size = size;
    backing->width = width;
    memset(backing->pixels, 0, size);
    DCFlushRange(backing->pixels, size);
    GX_InitTexObj(&backing->texture, backing->pixels,
                  (uint16_t)width, GLES_VRAM_HEIGHT,
                  GX_TF_RGB5A3, GX_CLAMP, GX_CLAMP, GX_FALSE);
    GX_InitTexObjFilterMode(&backing->texture, GX_NEAR, GX_NEAR);
    return 1;
}

static void GlesVramSetupCommon2D(int width, int height)
{
    Mtx44 projection;
    Mtx model;

    guOrtho(projection, 0.0f, (float)height,
            0.0f, (float)width, 0.0f, 1.0f);
    guMtxIdentity(model);
    GX_LoadProjectionMtx(projection, GX_ORTHOGRAPHIC);
    GX_LoadPosMtxImm(model, GX_PNMTX0);
    GX_SetCurrentMtx(GX_PNMTX0);
    GX_SetViewport(0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f);
    GX_SetScissor(0, 0, (uint32_t)width, (uint32_t)height);
    GX_SetCullMode(GX_CULL_NONE);
    GX_SetZMode(GX_FALSE, GX_ALWAYS, GX_FALSE);
    GX_SetZCompLoc(GX_TRUE);
    GX_SetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR);
    GX_SetAlphaCompare(GX_ALWAYS, 0, GX_AOP_AND, GX_ALWAYS, 0);
    GX_SetColorUpdate(GX_ENABLE);
    GX_SetAlphaUpdate(GX_ENABLE);
}

static void GlesVramSetupColor2D(int width, int height)
{
    GlesVramSetupCommon2D(width, height);
    GX_SetNumChans(1);
    GX_SetChanCtrl(GX_COLOR0A0, GX_DISABLE, GX_SRC_VTX, GX_SRC_VTX,
                   0, GX_DF_NONE, GX_AF_NONE);
    GX_SetNumTexGens(0);
    GX_SetNumTevStages(1);
    GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL,
                   GX_TEXMAP_NULL, GX_COLOR0A0);
    GX_ClearVtxDesc();
    GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
    GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
}

static void GlesVramDrawColorRect(float x0, float y0, float x1, float y1,
                                  GXColor color)
{
    g_vramTiling.gpuCommandsIssued = 1;
    GlesVramBeginNativeColorChannels();
    GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
    GX_Position3f32(x0, y0, 0.0f);
    GX_Color4u8(color.r, color.g, color.b, color.a);
    GX_Position3f32(x1, y0, 0.0f);
    GX_Color4u8(color.r, color.g, color.b, color.a);
    GX_Position3f32(x1, y1, 0.0f);
    GX_Color4u8(color.r, color.g, color.b, color.a);
    GX_Position3f32(x0, y1, 0.0f);
    GX_Color4u8(color.r, color.g, color.b, color.a);
    GX_End();
    GlesVramEndNativeColorChannels();
}

static void GlesVramClearWorkEfb(int width)
{
#ifdef GLES_VRAM_LR_TILING_S7_ANIM_DIAG
    if (width == GLES_VRAM_LEFT_WIDTH)
        g_vramTiling.s7AnimStats.clears[GLES_VRAM_TILE_LEFT]++;
    else if (width == GLES_VRAM_RIGHT_WIDTH)
        g_vramTiling.s7AnimStats.clears[GLES_VRAM_TILE_RIGHT]++;
#endif
    GlesVramSetupColor2D(width, GLES_VRAM_HEIGHT);
    GlesVramDrawColorRect(0.0f, 0.0f, (float)width,
                          (float)GLES_VRAM_HEIGHT,
                          (GXColor){0, 0, 0, 0});
}

static void GlesVramSetupTexture2D(int width, int height,
                                  GlesVramBacking *backing)
{
    GlesVramSetupCommon2D(width, height);

    /* Match OpenGX's dynamic-texture load sequence exactly.  Its custom
     * region callback always returns region 0, but GX_LoadTexObj() followed
     * by a global invalidate leaves the direct path outside OpenGX's region
     * residency protocol.  Explicitly invalidate the region before loading
     * the object, as checkLoadTextureObj() does for CPU-updated textures.
     *
     * texCacheRegionS[] contains cache regions (GX_InitTexCacheRegion), not
     * preloaded regions despite the API name, so no GX_TexModeSync is needed.
     */
    GX_InvalidateTexRegion(&texCacheRegionS[0]);
    GX_LoadTexObjPreloaded(&backing->texture, &texCacheRegionS[0],
                           GX_TEXMAP0);
    GX_SetNumChans(0);
    GX_SetNumTexGens(1);
    GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4,
                      GX_TG_TEX0, GX_IDENTITY);
    GX_SetNumTevStages(1);
    GX_SetTevOp(GX_TEVSTAGE0, GX_REPLACE);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0,
                   GX_TEXMAP0, GX_COLORNULL);
    GX_ClearVtxDesc();
    GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
    GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
}

static void GlesVramDrawTextureRect(GlesVramBacking *backing,
                                    int targetWidth, int targetHeight,
                                    float x0, float y0, float x1, float y1)
{
    g_vramTiling.gpuCommandsIssued = 1;
    GlesVramSetupTexture2D(targetWidth, targetHeight, backing);
    GlesVramBeginNativeColorChannels();
    GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
    GX_Position3f32(x0, y0, 0.0f);
    GX_TexCoord2f32(0.0f, 0.0f);
    GX_Position3f32(x1, y0, 0.0f);
    GX_TexCoord2f32(1.0f, 0.0f);
    GX_Position3f32(x1, y1, 0.0f);
    GX_TexCoord2f32(1.0f, 1.0f);
    GX_Position3f32(x0, y1, 0.0f);
    GX_TexCoord2f32(0.0f, 1.0f);
    GX_End();
    GlesVramEndNativeColorChannels();
}

#ifdef GLES_VRAM_LR_TILING_S4_EXPERIMENT
static void GlesVramDrawTextureSubRect(
    GlesVramBacking *backing, int targetWidth, int targetHeight,
    const GlesVramRect *source,
    float destX0, float destY0, float destX1, float destY1)
{
    float s0 = (float)source->x / (float)backing->width;
    float t0 = (float)source->y / (float)GLES_VRAM_HEIGHT;
    float s1 = (float)(source->x + source->width) /
               (float)backing->width;
    float t1 = (float)(source->y + source->height) /
               (float)GLES_VRAM_HEIGHT;

    g_vramTiling.gpuCommandsIssued = 1;
    GlesVramSetupTexture2D(targetWidth, targetHeight, backing);
    GlesVramBeginNativeColorChannels();
    GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
    GX_Position3f32(destX0, destY0, 0.0f);
    GX_TexCoord2f32(s0, t0);
    GX_Position3f32(destX1, destY0, 0.0f);
    GX_TexCoord2f32(s1, t0);
    GX_Position3f32(destX1, destY1, 0.0f);
    GX_TexCoord2f32(s1, t1);
    GX_Position3f32(destX0, destY1, 0.0f);
    GX_TexCoord2f32(s0, t1);
    GX_End();
    GlesVramEndNativeColorChannels();
}
#endif

#ifdef GLES_VRAM_LR_TILING_S3_EXPERIMENT
static uint16_t GlesVramReadPsx15(const unsigned short *cpuVram,
                                  int x, int y)
{
    const unsigned char *p = (const unsigned char *)(cpuVram +
                              y * GLES_VRAM_WIDTH + x);
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static void GlesVramWritePsx15(unsigned short *cpuVram,
                               int x, int y, uint16_t value)
{
    unsigned char *p = (unsigned char *)(cpuVram +
                       y * GLES_VRAM_WIDTH + x);
    p[0] = (unsigned char)value;
    p[1] = (unsigned char)(value >> 8);
}

static uint16_t GlesVramReadGXPixel(const GlesVramBacking *backing,
                                    int localX, int y)
{
    const unsigned char *bytes = (const unsigned char *)backing->pixels;
    size_t offset = GlesVramRGB5A3Offset(backing->width, localX, y);
    return (uint16_t)(((uint16_t)bytes[offset] << 8) |
                      bytes[offset + 1]);
}

#ifdef GLES_VRAM_LR_TILING_S3_TEST
static GXColor GlesVramS3DiagnosticColor(uint16_t gx)
{
    uint16_t psx = GlesVramGXRGB5A3ToPsx15(gx);
    GXColor color;
    color.r = (unsigned char)(((psx & 31u) * 255u + 15u) / 31u);
    color.g = (unsigned char)((((psx >> 5) & 31u) * 255u + 15u) / 31u);
    color.b = (unsigned char)((((psx >> 10) & 31u) * 255u + 15u) / 31u);
    color.a = 255;
    return color;
}

static uint16_t GlesVramS3DiagnosticGXColor(GXColor color)
{
    uint16_t psx = (uint16_t)((color.r >> 3) |
                              ((color.g >> 3) << 5) |
                              ((color.b >> 3) << 10));
    return GlesVramPsx15ToGXRGB5A3(psx);
}

static void GlesVramDrawS3Diagnostics(void)
{
    const uint16_t *rows[4];
    int row;
    int column;

    if (!g_vramTiling.s3DiagnosticsReady)
        return;
    rows[0] = g_vramTiling.s3CpuSamples;
    rows[1] = g_vramTiling.s3StagingSamples;
    rows[2] = g_vramTiling.s3EfbSamples;
    rows[3] = g_vramTiling.s3BackingSamples;
    GlesVramSetupColor2D(640, 480);
    for (row = 0; row < 4; row++)
    {
        for (column = 0; column < 4; column++)
        {
            float x0 = 24.0f + column * 24.0f;
            float y0 = 20.0f + row * 24.0f;
            GlesVramDrawColorRect(x0, y0, x0 + 20.0f, y0 + 20.0f,
                                  (GXColor){128, 128, 128, 255});
            GlesVramDrawColorRect(x0 + 2.0f, y0 + 2.0f,
                                  x0 + 18.0f, y0 + 18.0f,
                                  GlesVramS3DiagnosticColor(
                                      rows[row][column]));
        }
    }
}
#endif

static void GlesVramWriteStagingPixel(int textureWidth,
                                      int localX, int y, uint16_t value)
{
    unsigned char *bytes = (unsigned char *)g_vramTiling.staging;
    size_t offset = GlesVramRGB5A3Offset(textureWidth, localX, y);
    bytes[offset] = (unsigned char)(value >> 8);
    bytes[offset + 1] = (unsigned char)value;
}

static void GlesVramDrawStagingRun(int textureWidth,
                                   int destX0, int destY0,
                                   int destX1, int destY1,
                                   int sourceX0, int sourceY0,
                                   int sourceX1, int sourceY1)
{
    float s0 = (float)sourceX0 / (float)textureWidth;
    float s1 = (float)sourceX1 / (float)textureWidth;
    float t0 = (float)sourceY0 / (float)GLES_VRAM_HEIGHT;
    float t1 = (float)sourceY1 / (float)GLES_VRAM_HEIGHT;

    GlesVramBeginNativeColorChannels();
    GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
    GX_Position3f32((float)destX0, (float)destY0, 0.0f);
    GX_TexCoord2f32(s0, t0);
    GX_Position3f32((float)destX1, (float)destY0, 0.0f);
    GX_TexCoord2f32(s1, t0);
    GX_Position3f32((float)destX1, (float)destY1, 0.0f);
    GX_TexCoord2f32(s1, t1);
    GX_Position3f32((float)destX0, (float)destY1, 0.0f);
    GX_TexCoord2f32(s0, t1);
    GX_End();
    GlesVramEndNativeColorChannels();
}

#ifdef GLES_VRAM_LR_TILING_S7_DIRECT_BACKING_TEST
static void GlesVramSaveTileToBacking(GlesVramTileId tile);
static void GlesVramRestoreTileBackingToEfb(GlesVramTileId tile);

static void GlesVramWriteBackingPixel(GlesVramBacking *backing,
                                      int localX, int y, uint16_t value)
{
    unsigned char *bytes = (unsigned char *)backing->pixels;
    size_t offset = GlesVramRGB5A3Offset(backing->width, localX, y);
    bytes[offset] = (unsigned char)(value >> 8);
    bytes[offset + 1] = (unsigned char)value;
}
#endif

static int GlesVramUploadCpuBlocks(GlesVramTileId tile)
{
    GlesVramBacking staging;
    unsigned char upload[GLES_VRAM_BLOCKS_Y][GLES_VRAM_BLOCKS_X];
    unsigned char drawn[GLES_VRAM_BLOCKS_Y][GLES_VRAM_BLOCKS_X];
    short sourceSlot[GLES_VRAM_BLOCKS_Y][GLES_VRAM_BLOCKS_X];
    int blockX0;
    int blockX1;
    int tileOrigin;
    int tileWidth;
    int stagingWidth = GLES_VRAM_LEFT_WIDTH;
    int stagingBlocksPerRow = GLES_VRAM_LEFT_WIDTH /
                              GLES_VRAM_BLOCK_WIDTH;
    int textureSize;
    int blockX;
    int blockY;
    int stagingBlockRow = 0;
    int any = 0;
#ifdef GLES_VRAM_LR_TILING_S7_ANIM_DIAG
    GlesVramS7AnimStats *animStats = &g_vramTiling.s7AnimStats;
    int animTile = (int)tile;
    int animMinBlockX = GLES_VRAM_BLOCKS_X;
    int animMinBlockY = GLES_VRAM_BLOCKS_Y;
    int animMaxBlockX = -1;
    int animMaxBlockY = -1;
#endif

    if (psxVuw == NULL)
        return 0;
    tileOrigin = tile == GLES_VRAM_TILE_RIGHT ? GLES_VRAM_RIGHT_X : 0;
    tileWidth = g_vramTiling.backing[tile].width;
    blockX0 = tileOrigin / GLES_VRAM_BLOCK_WIDTH;
    blockX1 = (tileOrigin + tileWidth) / GLES_VRAM_BLOCK_WIDTH;
    memset(upload, 0, sizeof(upload));
    memset(drawn, 0, sizeof(drawn));
    memset(sourceSlot, 0xff, sizeof(sourceSlot));

    for (blockY = 0; blockY < GLES_VRAM_BLOCKS_Y; blockY++)
    {
        for (blockX = blockX0; blockX < blockX1; blockX++)
        {
            if (GlesVramManagerBlockNeedsCpuUpload(
                    &g_vramTiling.manager, blockX, blockY))
            {
                upload[blockY][blockX] = 1;
                any = 1;
#ifdef GLES_VRAM_LR_TILING_S7_ANIM_DIAG
                if (blockX < animMinBlockX) animMinBlockX = blockX;
                if (blockY < animMinBlockY) animMinBlockY = blockY;
                if (blockX > animMaxBlockX) animMaxBlockX = blockX;
                if (blockY > animMaxBlockY) animMaxBlockY = blockY;
#endif
            }
        }
    }
    if (!any)
        return 0;

#ifdef GLES_VRAM_LR_TILING_S7_ANIM_DIAG
    GlesVramS7AnimMergeBounds(
        &animStats->uploadBounds[animTile],
        animStats->uploadCalls[animTile],
        animMinBlockX * GLES_VRAM_BLOCK_WIDTH,
        animMinBlockY * GLES_VRAM_BLOCK_HEIGHT,
        (animMaxBlockX - animMinBlockX + 1) * GLES_VRAM_BLOCK_WIDTH,
        (animMaxBlockY - animMinBlockY + 1) * GLES_VRAM_BLOCK_HEIGHT);
    if (animStats->uploadCalls[animTile] == 0)
    {
        animStats->uploadSourceHash[animTile] = 2166136261u;
        animStats->uploadStagingHash[animTile] = 2166136261u;
    }
    animStats->uploadCalls[animTile]++;
#endif

#ifdef GLES_VRAM_LR_TILING_S7_DIRECT_BACKING_TEST
    {
        GlesVramBacking *backing = &g_vramTiling.backing[tile];
        int inflightTile;

        /* If this tile is already active, preserve unrelated GPU-new pixels
         * before replacing it with the CPU-patched backing. */
        if (g_vramTiling.manager.activeTile == tile &&
            g_vramTiling.manager.efbDirty &&
            GlesVramManagerPersistActive(&g_vramTiling.manager) == tile)
            GlesVramSaveTileToBacking(tile);

        /* The target backing may still be a GX copy destination or texture
         * source from the preceding tile restore.  Finish those references
         * before the CPU patches its authoritative RGB5A3 blocks. */
        GX_DrawDone();
        g_vramTiling.stagingInFlight = 0;
        for (inflightTile = 0; inflightTile < 2; inflightTile++)
        {
            if (g_vramTiling.backingInFlight[inflightTile])
            {
                DCInvalidateRange(
                    g_vramTiling.backing[inflightTile].pixels,
                    g_vramTiling.backing[inflightTile].size);
                g_vramTiling.backingInFlight[inflightTile] = 0;
            }
        }
        DCInvalidateRange(backing->pixels, backing->size);

        for (blockY = 0; blockY < GLES_VRAM_BLOCKS_Y; blockY++)
        {
            for (blockX = blockX0; blockX < blockX1; blockX++)
            {
                int x;
                int y;
                if (!upload[blockY][blockX])
                    continue;
                for (y = blockY * GLES_VRAM_BLOCK_HEIGHT;
                     y < (blockY + 1) * GLES_VRAM_BLOCK_HEIGHT; y++)
                {
                    for (x = blockX * GLES_VRAM_BLOCK_WIDTH;
                         x < (blockX + 1) * GLES_VRAM_BLOCK_WIDTH; x++)
                    {
                        uint16_t converted = GlesVramPsx15ToGXRGB5A3(
                            GlesVramReadPsx15(psxVuw, x, y));
                        GlesVramWriteBackingPixel(
                            backing, x - tileOrigin, y, converted);
#ifdef GLES_VRAM_LR_TILING_S7_ANIM_DIAG
                        animStats->uploadSourceHash[animTile] =
                            GlesVramS7AnimHashPixel(
                                animStats->uploadSourceHash[animTile],
                                converted);
#endif
                    }
                }
#ifdef GLES_VRAM_LR_TILING_S7_ANIM_DIAG
                animStats->uploadBlocks[animTile]++;
#endif
            }
        }
        DCFlushRange(backing->pixels, backing->size);

#ifdef GLES_VRAM_LR_TILING_S7_ANIM_DIAG
        for (blockY = 0; blockY < GLES_VRAM_BLOCKS_Y; blockY++)
        {
            for (blockX = blockX0; blockX < blockX1; blockX++)
            {
                int x;
                int y;
                if (!upload[blockY][blockX])
                    continue;
                for (y = blockY * GLES_VRAM_BLOCK_HEIGHT;
                     y < (blockY + 1) * GLES_VRAM_BLOCK_HEIGHT; y++)
                {
                    for (x = blockX * GLES_VRAM_BLOCK_WIDTH;
                         x < (blockX + 1) * GLES_VRAM_BLOCK_WIDTH; x++)
                    {
                        animStats->uploadStagingHash[animTile] =
                            GlesVramS7AnimHashPixel(
                                animStats->uploadStagingHash[animTile],
                                GlesVramReadGXPixel(
                                    backing, x - tileOrigin, y));
                    }
                }
            }
        }
        /* In this A/B build, one run denotes the single full-backing restore
         * which replaces all staging upload rectangles. */
        animStats->uploadRuns[animTile]++;
#endif

        for (blockY = 0; blockY < GLES_VRAM_BLOCKS_Y; blockY++)
        {
            for (blockX = blockX0; blockX < blockX1; blockX++)
            {
                if (upload[blockY][blockX])
                {
                    uint64_t sequence =
                        g_vramTiling.manager.block[blockY][blockX].cpuSequence;
#ifdef GLES_VRAM_LR_TILING_S7_ANIM_DIAG
                    animStats->uploadCommits[animTile]++;
                    if (!GlesVramManagerCommitCpuUpload(
                            &g_vramTiling.manager,
                            blockX, blockY, sequence))
                        animStats->uploadCommitFailures[animTile]++;
#else
                    GlesVramManagerCommitCpuUpload(
                        &g_vramTiling.manager, blockX, blockY, sequence);
#endif
                }
            }
        }

        g_vramTiling.manager.backingValid[tile] = 1;
        g_vramTiling.manager.efbDirty = 0;
        GX_InvalidateTexAll();
        GlesVramRestoreTileBackingToEfb(tile);
        glInvalidateGXState();
        return 1;
    }
#endif

    /* staging is a texture source.  Do not let the CPU overwrite it while a
     * previous upload draw can still be sampling the old bytes. */
    if (g_vramTiling.stagingInFlight)
    {
        GX_DrawDone();
        g_vramTiling.stagingInFlight = 0;
    }

    for (blockY = 0; blockY < GLES_VRAM_BLOCKS_Y; blockY++)
    {
        int stagingBlockColumn = 0;
        for (blockX = blockX0; blockX < blockX1; blockX++)
        {
            int x;
            int y;
            int slot;
            int sourceX;
            int sourceY;
            if (!upload[blockY][blockX])
                continue;
            /* Keep every destination block row within one atlas row.  A
             * globally packed slot stream can wrap a destination row at the
             * 40-block staging boundary, forcing adjacent destination pixels
             * to be emitted by unrelated quads from different source rows.
             * Row-local packing retains the proven low-X compact atlas while
             * removing those artificial splits. */
            slot = stagingBlockRow * stagingBlocksPerRow +
                   stagingBlockColumn++;
            sourceSlot[blockY][blockX] = (short)slot;
            sourceX = (slot % stagingBlocksPerRow) *
                      GLES_VRAM_BLOCK_WIDTH;
            sourceY = (slot / stagingBlocksPerRow) *
                      GLES_VRAM_BLOCK_HEIGHT;
            for (y = blockY * GLES_VRAM_BLOCK_HEIGHT;
                 y < (blockY + 1) * GLES_VRAM_BLOCK_HEIGHT; y++)
            {
                for (x = blockX * GLES_VRAM_BLOCK_WIDTH;
                     x < (blockX + 1) * GLES_VRAM_BLOCK_WIDTH; x++)
                {
                    uint16_t converted = GlesVramPsx15ToGXRGB5A3(
                        GlesVramReadPsx15(psxVuw, x, y));
                    GlesVramWriteStagingPixel(
                        stagingWidth,
                        sourceX + (x & (GLES_VRAM_BLOCK_WIDTH - 1)),
                        sourceY + (y & (GLES_VRAM_BLOCK_HEIGHT - 1)),
                        converted);
#ifdef GLES_VRAM_LR_TILING_S7_ANIM_DIAG
                    animStats->uploadSourceHash[animTile] =
                        GlesVramS7AnimHashPixel(
                            animStats->uploadSourceHash[animTile], converted);
#endif
                }
            }
#ifdef GLES_VRAM_LR_TILING_S7_ANIM_DIAG
            animStats->uploadBlocks[animTile]++;
#endif
        }
        if (stagingBlockColumn != 0)
            stagingBlockRow++;
    }

    textureSize = (int)GX_GetTexBufferSize((uint16_t)stagingWidth,
                                            GLES_VRAM_HEIGHT,
                                            GX_TF_RGB5A3, 0, GX_FALSE);
    DCFlushRange(g_vramTiling.staging, (uint32_t)textureSize);
    memset(&staging, 0, sizeof(staging));
    staging.pixels = g_vramTiling.staging;
    staging.size = (uint32_t)textureSize;
    staging.width = stagingWidth;
    GX_InitTexObj(&staging.texture, staging.pixels,
                  (uint16_t)stagingWidth, GLES_VRAM_HEIGHT,
                  GX_TF_RGB5A3, GX_CLAMP, GX_CLAMP, GX_FALSE);
    GX_InitTexObjFilterMode(&staging.texture, GX_NEAR, GX_NEAR);
    GlesVramSetupTexture2D(tileWidth, GLES_VRAM_HEIGHT, &staging);

#ifdef GLES_VRAM_LR_TILING_S3_TEST
    {
        int diagnosticBlockX = tile == GLES_VRAM_TILE_LEFT ? 39 : 40;
        int diagnosticBlockY = 7;
        int slot = sourceSlot[diagnosticBlockY][diagnosticBlockX];
        if (slot >= 0)
        {
            int sourceX = (slot % stagingBlocksPerRow) *
                          GLES_VRAM_BLOCK_WIDTH;
            int sourceY = (slot / stagingBlocksPerRow) *
                          GLES_VRAM_BLOCK_HEIGHT;
            int base = tile == GLES_VRAM_TILE_LEFT ? 0 : 2;
            g_vramTiling.s3StagingSamples[base] =
                GlesVramReadGXPixel(&staging, sourceX, sourceY);
            g_vramTiling.s3StagingSamples[base + 1] =
                GlesVramReadGXPixel(&staging, sourceX + 8, sourceY);
        }
    }
#endif

#ifdef GLES_VRAM_LR_TILING_S7_ANIM_DIAG
    /* Read the packed staging slots back in exactly the same block/pixel
     * order as the source hash.  A mismatch isolates corruption before any
     * EFB draw or backing copy is involved. */
    for (blockY = 0; blockY < GLES_VRAM_BLOCKS_Y; blockY++)
    {
        for (blockX = blockX0; blockX < blockX1; blockX++)
        {
            int x;
            int y;
            int slot;
            int sourceX;
            int sourceY;
            if (!upload[blockY][blockX])
                continue;
            slot = sourceSlot[blockY][blockX];
            sourceX = (slot % stagingBlocksPerRow) *
                      GLES_VRAM_BLOCK_WIDTH;
            sourceY = (slot / stagingBlocksPerRow) *
                      GLES_VRAM_BLOCK_HEIGHT;
            for (y = 0; y < GLES_VRAM_BLOCK_HEIGHT; y++)
            {
                for (x = 0; x < GLES_VRAM_BLOCK_WIDTH; x++)
                {
                    animStats->uploadStagingHash[animTile] =
                        GlesVramS7AnimHashPixel(
                            animStats->uploadStagingHash[animTile],
                            GlesVramReadGXPixel(
                                &staging, sourceX + x, sourceY + y));
                }
            }
        }
    }
#endif

    for (blockY = 0; blockY < GLES_VRAM_BLOCKS_Y; blockY++)
    {
        blockX = blockX0;
        while (blockX < blockX1)
        {
            int runStart;
            int runEnd;
            int runBlockYEnd;
            int runSlot;
            int previousSlot;
            int sourceX0;
            int sourceY0;
            int drawBlockY;
            int drawBlockX;
            while (blockX < blockX1 &&
                   (!upload[blockY][blockX] ||
                    drawn[blockY][blockX]))
                blockX++;
            if (blockX >= blockX1)
                break;
            runStart = blockX;
            runSlot = sourceSlot[blockY][blockX];
            previousSlot = runSlot;
            blockX++;
            while (blockX < blockX1 && upload[blockY][blockX] &&
                   !drawn[blockY][blockX] &&
                   sourceSlot[blockY][blockX] == previousSlot + 1 &&
                   sourceSlot[blockY][blockX] / stagingBlocksPerRow ==
                       runSlot / stagingBlocksPerRow)
            {
                previousSlot = sourceSlot[blockY][blockX];
                blockX++;
            }
            runEnd = blockX;
            runBlockYEnd = blockY + 1;

            /* Extend an identical horizontal run through consecutive target
             * block rows when its row-local atlas source is also consecutive.
             * This removes the remaining 16-pixel quad boundaries without
             * touching gaps or widening a sparse CPU upload. */
            while (runBlockYEnd < GLES_VRAM_BLOCKS_Y)
            {
                int expectedSlot = runSlot +
                    (runBlockYEnd - blockY) * stagingBlocksPerRow;
                int canMerge = 1;
                for (drawBlockX = runStart;
                     drawBlockX < runEnd; drawBlockX++)
                {
                    if (!upload[runBlockYEnd][drawBlockX] ||
                        drawn[runBlockYEnd][drawBlockX] ||
                        sourceSlot[runBlockYEnd][drawBlockX] !=
                            expectedSlot + (drawBlockX - runStart))
                    {
                        canMerge = 0;
                        break;
                    }
                }
                if (!canMerge)
                    break;
                runBlockYEnd++;
            }

            for (drawBlockY = blockY;
                 drawBlockY < runBlockYEnd; drawBlockY++)
            {
                for (drawBlockX = runStart;
                     drawBlockX < runEnd; drawBlockX++)
                    drawn[drawBlockY][drawBlockX] = 1;
            }
            sourceX0 = (runSlot % stagingBlocksPerRow) *
                       GLES_VRAM_BLOCK_WIDTH;
            sourceY0 = (runSlot / stagingBlocksPerRow) *
                       GLES_VRAM_BLOCK_HEIGHT;
#ifdef GLES_VRAM_LR_TILING_S7_ANIM_DIAG
            animStats->uploadRuns[animTile]++;
#endif
            GlesVramDrawStagingRun(
                stagingWidth,
                runStart * GLES_VRAM_BLOCK_WIDTH - tileOrigin,
                blockY * GLES_VRAM_BLOCK_HEIGHT,
                runEnd * GLES_VRAM_BLOCK_WIDTH - tileOrigin,
                runBlockYEnd * GLES_VRAM_BLOCK_HEIGHT,
                sourceX0, sourceY0,
                sourceX0 + (runEnd - runStart) *
                    GLES_VRAM_BLOCK_WIDTH,
                sourceY0 + (runBlockYEnd - blockY) *
                    GLES_VRAM_BLOCK_HEIGHT);
        }
    }

    g_vramTiling.gpuCommandsIssued = 1;
    GX_PixModeSync();
    g_vramTiling.stagingInFlight = 1;
    for (blockY = 0; blockY < GLES_VRAM_BLOCKS_Y; blockY++)
    {
        for (blockX = blockX0; blockX < blockX1; blockX++)
        {
            if (upload[blockY][blockX])
            {
                uint64_t sequence =
                    g_vramTiling.manager.block[blockY][blockX].cpuSequence;
#ifdef GLES_VRAM_LR_TILING_S7_ANIM_DIAG
                animStats->uploadCommits[animTile]++;
                if (!GlesVramManagerCommitCpuUpload(
                        &g_vramTiling.manager, blockX, blockY, sequence))
                    animStats->uploadCommitFailures[animTile]++;
#else
                GlesVramManagerCommitCpuUpload(
                    &g_vramTiling.manager, blockX, blockY, sequence);
#endif
            }
        }
    }
    GlesVramManagerMarkActiveDirty(&g_vramTiling.manager);
    glInvalidateGXState();
    return 1;
}
#endif

static void GlesVramSaveTileToBacking(GlesVramTileId tile)
{
    GlesVramBacking *backing;

    if (tile != GLES_VRAM_TILE_LEFT && tile != GLES_VRAM_TILE_RIGHT)
        return;

#ifdef GLES_VRAM_LR_TILING_S7_ANIM_DIAG
    g_vramTiling.s7AnimStats.saveCopies[tile]++;
#endif

    backing = &g_vramTiling.backing[tile];
    GX_SetCopyFilter(GX_FALSE, NULL, GX_FALSE, NULL);
    GX_SetTexCopySrc(0, 0, (uint16_t)backing->width, GLES_VRAM_HEIGHT);
    GX_SetTexCopyDst((uint16_t)backing->width, GLES_VRAM_HEIGHT,
                     GX_TF_RGB5A3, GX_FALSE);
    g_vramTiling.gpuCommandsIssued = 1;
    GX_CopyTex(MEM_K0_TO_K1(backing->pixels), GX_FALSE);
    g_vramTiling.backingInFlight[tile] = 1;
    /* FIFO ordering is sufficient for a following GX texture restore.  This
     * sync command orders pixel operations but does not stall the CPU. */
    GX_PixModeSync();
    RestoreDispCopyInfo();
}

static void GlesVramRestoreTileBackingToEfb(GlesVramTileId tile)
{
    GlesVramBacking *backing = &g_vramTiling.backing[tile];

#ifdef GLES_VRAM_LR_TILING_S7_ANIM_DIAG
    g_vramTiling.s7AnimStats.restores[tile]++;
#endif

    GlesVramDrawTextureRect(backing, backing->width, GLES_VRAM_HEIGHT,
                            0.0f, 0.0f, (float)backing->width,
                            (float)GLES_VRAM_HEIGHT);
}

int GlesVramTilingInitialize(void)
{
    uint32_t stagingSize;

    if (g_vramTiling.initialized)
        return 1;

    memset(&g_vramTiling, 0, sizeof(g_vramTiling));
    if (!GlesVramAllocateBacking(&g_vramTiling.backing[GLES_VRAM_TILE_LEFT],
                                 GLES_VRAM_LEFT_WIDTH) ||
        !GlesVramAllocateBacking(&g_vramTiling.backing[GLES_VRAM_TILE_RIGHT],
                                 GLES_VRAM_RIGHT_WIDTH)
#ifdef GLES_VRAM_LR_TILING_S7_PIPELINE_DIAG
        || !GlesVramAllocateBacking(&g_vramTiling.diagEfb,
                                    GLES_VRAM_LEFT_WIDTH)
#endif
        )
    {
        GlesVramTilingShutdown();
        return 0;
    }

    stagingSize = g_vramTiling.backing[GLES_VRAM_TILE_LEFT].size;
    g_vramTiling.staging = _mem2_memalign(32, stagingSize);
    if (g_vramTiling.staging == NULL)
    {
        GlesVramTilingShutdown();
        return 0;
    }
    g_vramTiling.stagingSize = stagingSize;
    memset(g_vramTiling.staging, 0, stagingSize);
    DCFlushRange(g_vramTiling.staging, stagingSize);

    g_vramTiling.initialized = 1;
    GlesVramTilingReset();
    return 1;
}

void GlesVramTilingReset(void)
{
    if (!g_vramTiling.initialized)
        return;

    GlesVramManagerReset(&g_vramTiling.manager);
#ifdef GLES_VRAM_LR_TILING_S7_EXPERIMENT
    GlesVramFrontPresentationReset(&g_vramTiling.frontPresentation);
#ifdef GLES_VRAM_LR_TILING_S7_ANIM_DIAG
    memset(&g_vramTiling.s7AnimStats, 0,
           sizeof(g_vramTiling.s7AnimStats));
#ifdef GLES_VRAM_LR_TILING_S7_PIPELINE_DIAG
    g_vramTiling.diagPresentFrame = 0;
    g_vramTiling.diagXfbFrame = 0;
    g_vramTiling.diagXfbPixels = NULL;
    g_vramTiling.diagXfbWidth = 0;
    g_vramTiling.diagXfbHeight = 0;
    g_vramTiling.diagXfbStridePixels = 0;
    g_vramTiling.diagXfbInFlight = 0;
    g_vramTiling.diagXfbReady = 0;
#endif
#endif
#endif
#ifdef GLES_VRAM_LR_TILING_S1_TEST
    g_vramTiling.visualTestReady = 0;
#endif
#ifdef GLES_VRAM_LR_TILING_S2_EXPERIMENT
    memset(&g_vramTiling.s2Plan, 0, sizeof(g_vramTiling.s2Plan));
    g_vramTiling.s2Sequence = 0;
    g_vramTiling.s2Active = 0;
    g_vramTiling.s2PassIndex = -1;
    g_vramTiling.s2PassSubmitted = 0;
#ifdef GLES_VRAM_LR_TILING_S7_EXPERIMENT
    g_vramTiling.s2FrontDisplayValid = 0;
#endif
#ifdef GLES_VRAM_LR_TILING_S2_TEST
    g_vramTiling.s2VisualTestReady = 0;
#endif
#ifdef GLES_VRAM_LR_TILING_S3_TEST
    g_vramTiling.s3VisualTestReady = 0;
    g_vramTiling.s3DiagnosticsReady = 0;
#endif
#ifdef GLES_VRAM_LR_TILING_S4_TEST
    g_vramTiling.s4VisualTestReady = 0;
#endif
#ifdef GLES_VRAM_LR_TILING_S3_EXPERIMENT
    g_vramTiling.s3CpuWritePrepared = 0;
#endif
#endif
#ifdef GLES_VRAM_LR_TILING_S5_EXPERIMENT
    g_vramTiling.s5MoveCount = 0;
    g_vramTiling.s5C0Count = 0;
    g_vramTiling.s5ResolveCalls = 0;
    g_vramTiling.s5ResolveWaits = 0;
    g_vramTiling.s5ResolvedBlocks = 0;
#endif
}

void GlesVramTilingShutdown(void)
{
    int tile;

    /* Frame-time save/restore remains fully asynchronous.  Only resource
     * destruction waits, so GX can no longer reference memory being freed. */
    if (g_vramTiling.gpuCommandsIssued)
        GX_DrawDone();

    for (tile = 0; tile < 2; tile++)
    {
        if (g_vramTiling.backing[tile].pixels != NULL)
            _mem2_free(g_vramTiling.backing[tile].pixels);
    }
#ifdef GLES_VRAM_LR_TILING_S7_PIPELINE_DIAG
    if (g_vramTiling.diagEfb.pixels != NULL)
        _mem2_free(g_vramTiling.diagEfb.pixels);
#endif
    if (g_vramTiling.staging != NULL)
        _mem2_free(g_vramTiling.staging);
#ifdef GLES_VRAM_LR_TILING_S5_EXPERIMENT
    if (g_vramTiling.moveScratch != NULL)
        _mem2_free(g_vramTiling.moveScratch);
#endif
    memset(&g_vramTiling, 0, sizeof(g_vramTiling));
    g_vramTiling.manager.activeTile = GLES_VRAM_TILE_NONE;
}

size_t GlesVramTilingAllocatedBytes(void)
{
    size_t bytes =
           (size_t)g_vramTiling.backing[GLES_VRAM_TILE_LEFT].size +
           (size_t)g_vramTiling.backing[GLES_VRAM_TILE_RIGHT].size +
           (size_t)g_vramTiling.stagingSize;
#ifdef GLES_VRAM_LR_TILING_S7_PIPELINE_DIAG
    bytes += (size_t)g_vramTiling.diagEfb.size;
#endif
#ifdef GLES_VRAM_LR_TILING_S5_EXPERIMENT
    bytes += g_vramTiling.moveScratchPixels * sizeof(uint16_t);
#endif
    return bytes;
}

int GlesVramTilingIsInitialized(void)
{
    return g_vramTiling.initialized;
}

int GlesVramTilingSelectTile(GlesVramTileId tile)
{
    unsigned int actions;
    GlesVramTileId oldTile;

    if (!g_vramTiling.initialized ||
        (tile != GLES_VRAM_TILE_LEFT && tile != GLES_VRAM_TILE_RIGHT))
        return 0;

    oldTile = g_vramTiling.manager.activeTile;
#ifdef GLES_VRAM_LR_TILING_S7_ANIM_DIAG
    g_vramTiling.s7AnimStats.tileSelects[tile]++;
#endif
    actions = GlesVramManagerSelectTile(&g_vramTiling.manager, tile);
    if (actions & GLES_VRAM_SWITCH_SAVE)
        GlesVramSaveTileToBacking(oldTile);
    if (actions & GLES_VRAM_SWITCH_RESTORE)
        GlesVramRestoreTileBackingToEfb(tile);
    else if (actions & GLES_VRAM_SWITCH_CLEAR)
        GlesVramClearWorkEfb(g_vramTiling.backing[tile].width);

#ifdef GLES_VRAM_LR_TILING_S3_EXPERIMENT
    GlesVramUploadCpuBlocks(tile);
#endif

    if (actions != 0)
        glInvalidateGXState();
    return 1;
}

void GlesVramTilingMarkActiveDirty(void)
{
    GlesVramManagerMarkActiveDirty(&g_vramTiling.manager);
}

void GlesVramTilingDiscardActive(void)
{
    GlesVramManagerDiscardActive(&g_vramTiling.manager);
}

GlesVramTileId GlesVramTilingActiveTile(void)
{
    return g_vramTiling.manager.activeTile;
}

#ifdef GLES_VRAM_LR_TILING_S2_EXPERIMENT
int GlesVramTilingS2BeginCommand(unsigned char command,
                                 const void *packet, int packetWords,
                                 const GlesVramRect *drawArea,
                                 const GlesVramPoint *drawOffset,
                                 const GlesVramRect *frontDisplay)
{
    if (!g_vramTiling.initialized || g_vramTiling.s2Active)
        return 0;
#ifdef GLES_VRAM_LR_TILING_S2_TEST
    /* Once the synthetic result is ready, keep ordinary game drawing on the
     * legacy path so it cannot alter the diagnostic backing textures. */
    if (g_vramTiling.s2VisualTestReady)
        return 0;
#endif
#ifdef GLES_VRAM_LR_TILING_S3_TEST
    if (g_vramTiling.s3VisualTestReady)
        return 0;
#endif
    if (!GlesVramBuildPrimitivePlan(command, packet, packetWords,
                                    drawArea, drawOffset,
                                    &g_vramTiling.s2Plan))
        return 0;
#ifdef GLES_VRAM_LR_TILING_S3_EXPERIMENT
    if (g_vramTiling.s2Plan.isFill &&
        !GlesVramTilingS3PrepareCpuWrite(
            psxVuw, &g_vramTiling.s2Plan.writeRect))
    {
        memset(&g_vramTiling.s2Plan, 0, sizeof(g_vramTiling.s2Plan));
        return 0;
    }
#endif

    g_vramTiling.s2Sequence =
        GlesVramManagerBeginWrite(&g_vramTiling.manager);
    if (g_vramTiling.s2Sequence == 0)
    {
#ifdef GLES_VRAM_LR_TILING_S3_EXPERIMENT
        g_vramTiling.s3CpuWritePrepared = 0;
#endif
        return 0;
    }
    g_vramTiling.s2PassSubmitted = 0;
    g_vramTiling.s2PassIndex = -1;
#ifdef GLES_VRAM_LR_TILING_S7_EXPERIMENT
    if (frontDisplay != NULL)
    {
        g_vramTiling.s2FrontDisplay = *frontDisplay;
        g_vramTiling.s2FrontDisplayValid = 1;
    }
    else
        g_vramTiling.s2FrontDisplayValid = 0;
#else
    (void)frontDisplay;
#endif
    g_vramTiling.s2Active = 1;
    return 1;
}

void GlesVramTilingS2EndCommand(void)
{
    if (!g_vramTiling.s2Active)
        return;

    /* primBlkFill preserves the existing psxVuw software mirror.  Mark it as
     * the same generation as the GPU copies rather than inventing a second
     * logical write. */
    if (g_vramTiling.s2Plan.isFill)
    {
#ifdef GLES_VRAM_LR_TILING_S3_EXPERIMENT
        if (g_vramTiling.s3CpuWritePrepared)
        {
            g_vramTiling.s3CpuWritePrepared = 0;
            GlesVramManagerMarkCpuWrite(&g_vramTiling.manager,
                                        &g_vramTiling.s2Plan.writeRect,
                                        g_vramTiling.s2Sequence);
        }
#else
        GlesVramManagerMarkCpuWrite(&g_vramTiling.manager,
                                    &g_vramTiling.s2Plan.writeRect,
                                    g_vramTiling.s2Sequence);
#endif
    }

    memset(&g_vramTiling.s2Plan, 0, sizeof(g_vramTiling.s2Plan));
    g_vramTiling.s2Sequence = 0;
    g_vramTiling.s2PassIndex = -1;
    g_vramTiling.s2PassSubmitted = 0;
#ifdef GLES_VRAM_LR_TILING_S7_EXPERIMENT
    g_vramTiling.s2FrontDisplayValid = 0;
#endif
    g_vramTiling.s2Active = 0;
}

int GlesVramTilingS2CommandActive(void)
{
    return g_vramTiling.s2Active;
}

int GlesVramTilingS2PassCount(void)
{
    return g_vramTiling.s2Active ? g_vramTiling.s2Plan.spanCount : 0;
}

int GlesVramTilingS2PreparePass(int passIndex,
                                GlesVramTileSpan *span,
                                int *isFill)
{
    const GlesVramTileSpan *selected;
    int tileOrigin;
    int tileWidth;

    if (!g_vramTiling.s2Active || passIndex < 0 ||
        passIndex >= g_vramTiling.s2Plan.spanCount)
        return 0;

    selected = &g_vramTiling.s2Plan.span[passIndex];
    if (!GlesVramTilingSelectTile(selected->tile))
        return 0;

    tileOrigin = selected->tile == GLES_VRAM_TILE_RIGHT ?
                 GLES_VRAM_RIGHT_X : 0;
    tileWidth = g_vramTiling.backing[selected->tile].width;

    /* Keep vertices in absolute PS VRAM coordinates.  A tile-specific
     * projection maps [origin, origin+width) onto its local work EFB, while
     * the non-overlapping local scissor clips the duplicated submission. */
    glViewport(0, 0, tileWidth, GLES_VRAM_HEIGHT);
    glScissor(selected->localRect.x, selected->localRect.y,
              selected->localRect.width, selected->localRect.height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(tileOrigin, tileOrigin + tileWidth,
            GLES_VRAM_HEIGHT, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glSetLoadMtxFlg();

    g_vramTiling.s2PassSubmitted = 0;
    g_vramTiling.s2PassIndex = passIndex;
    if (span != NULL)
        *span = *selected;
    if (isFill != NULL)
        *isFill = g_vramTiling.s2Plan.isFill;
    return 1;
}

void GlesVramTilingS2NotifyDrawSubmitted(void)
{
    if (g_vramTiling.s2Active)
        g_vramTiling.s2PassSubmitted = 1;
}

void GlesVramTilingS2FinishPass(void)
{
    const GlesVramTileSpan *span;

    if (!g_vramTiling.s2Active || !g_vramTiling.s2PassSubmitted ||
        g_vramTiling.s2PassIndex < 0 ||
        g_vramTiling.s2PassIndex >= g_vramTiling.s2Plan.spanCount)
        return;

    span = &g_vramTiling.s2Plan.span[g_vramTiling.s2PassIndex];
#ifdef GLES_VRAM_LR_TILING_S7_ANIM_DIAG
    {
        int animTile = (int)span->tile;
        GlesVramS7AnimMergeBounds(
            &g_vramTiling.s7AnimStats.gpuBounds[animTile],
            g_vramTiling.s7AnimStats.gpuPasses[animTile],
            span->vramRect.x, span->vramRect.y,
            span->vramRect.width, span->vramRect.height);
        g_vramTiling.s7AnimStats.gpuPasses[animTile]++;
    }
#endif
    GlesVramManagerMarkGpuWrite(&g_vramTiling.manager,
                                &span->vramRect,
                                g_vramTiling.s2Sequence);
#ifdef GLES_VRAM_LR_TILING_S7_EXPERIMENT
    if (g_vramTiling.s2FrontDisplayValid)
        GlesVramFrontPresentationMarkWrite(
            &g_vramTiling.frontPresentation,
            &span->vramRect,
            &g_vramTiling.s2FrontDisplay);
#endif
    GlesVramManagerMarkActiveDirty(&g_vramTiling.manager);
    g_vramTiling.s2PassSubmitted = 0;
    g_vramTiling.s2PassIndex = -1;
}

void GlesVramTilingS2SaveAndDiscard(void)
{
    GlesVramTileId tile =
        GlesVramManagerSaveAndDiscardActive(&g_vramTiling.manager);
    if (tile != GLES_VRAM_TILE_NONE)
        GlesVramSaveTileToBacking(tile);
    glInvalidateGXState();
}

#ifdef GLES_VRAM_LR_TILING_S7_EXPERIMENT
int GlesVramTilingS7SelectPresentation(
    const GlesVramRect *requestedDisplay,
    GlesVramRect *selectedDisplay)
{
    return GlesVramFrontPresentationSelect(
        &g_vramTiling.frontPresentation,
        requestedDisplay, selectedDisplay);
}

void GlesVramTilingS7DiscardFrontPresentation(void)
{
    GlesVramFrontPresentationReset(&g_vramTiling.frontPresentation);
}

#ifdef GLES_VRAM_LR_TILING_S7_ANIM_DIAG
void GlesVramTilingS7AnimGetAndResetStats(GlesVramS7AnimStats *stats)
{
    if (stats == NULL)
        return;
    *stats = g_vramTiling.s7AnimStats;
    memset(&g_vramTiling.s7AnimStats, 0,
           sizeof(g_vramTiling.s7AnimStats));
}

#ifdef GLES_VRAM_LR_TILING_S7_PIPELINE_DIAG
void GlesVramTilingS7DiagSetPresentFrame(uint32_t frame)
{
    g_vramTiling.diagPresentFrame = frame;
}

void GlesVramTilingS7DiagBeginXfb(void *pixels, int width, int height,
                                  int stridePixels)
{
    g_vramTiling.diagXfbFrame = g_vramTiling.diagPresentFrame;
    g_vramTiling.diagXfbPixels = pixels;
    g_vramTiling.diagXfbWidth = width;
    g_vramTiling.diagXfbHeight = height;
    g_vramTiling.diagXfbStridePixels = stridePixels;
    g_vramTiling.diagXfbInFlight = pixels != NULL && width > 0 &&
        height > 0 && stridePixels >= width;
}

void GlesVramTilingS7DiagCompleteXfb(void)
{
    if (g_vramTiling.diagXfbInFlight)
    {
        g_vramTiling.diagXfbInFlight = 0;
        g_vramTiling.diagXfbReady = 1;
    }
}

int GlesVramTilingS7DiagGetXfbStats(GlesVramS7XfbStats *stats)
{
    unsigned char *bytes;
    uint32_t hash = 2166136261u;
    uint32_t lumaHash = 2166136261u;
    uint32_t active = 0;
    uint32_t byteCount;
    int x;
    int y;
    unsigned char lumaMin = 255;
    unsigned char lumaMax = 0;

    if (stats == NULL || !g_vramTiling.diagXfbReady ||
        g_vramTiling.diagXfbPixels == NULL)
        return 0;
    memset(stats, 0, sizeof(*stats));
    byteCount = (uint32_t)g_vramTiling.diagXfbStridePixels *
                (uint32_t)g_vramTiling.diagXfbHeight * 2u;
    bytes = (unsigned char *)MEM_K1_TO_K0(g_vramTiling.diagXfbPixels);
    DCInvalidateRange(bytes, byteCount);
    for (y = 0; y < g_vramTiling.diagXfbHeight; y++)
    {
        const unsigned char *row = bytes +
            (size_t)y * g_vramTiling.diagXfbStridePixels * 2u;
        for (x = 0; x < g_vramTiling.diagXfbStridePixels * 2; x++)
        {
            hash ^= row[x];
            hash *= 16777619u;
        }
        for (x = 0; x < g_vramTiling.diagXfbWidth; x++)
        {
            unsigned char luma = row[x * 2];
            lumaHash ^= luma;
            lumaHash *= 16777619u;
            if (luma < lumaMin) lumaMin = luma;
            if (luma > lumaMax) lumaMax = luma;
            if (luma > 24)
                active++;
        }
    }
    stats->valid = 1;
    stats->frame = g_vramTiling.diagXfbFrame;
    stats->hash = hash;
    stats->lumaHash = lumaHash;
    stats->lumaActive = active;
    stats->byteCount = byteCount;
    stats->width = (uint32_t)g_vramTiling.diagXfbWidth;
    stats->height = (uint32_t)g_vramTiling.diagXfbHeight;
    stats->stridePixels = (uint32_t)g_vramTiling.diagXfbStridePixels;
    stats->lumaMin = lumaMin;
    stats->lumaMax = lumaMax;
    g_vramTiling.diagXfbReady = 0;
    g_vramTiling.diagXfbPixels = NULL;
    return 1;
}
#endif
#endif
#endif

void GlesVramTilingS2DrawDebugComposite(void)
{
    GlesVramSetupColor2D(640, 480);
    GlesVramDrawColorRect(0.0f, 0.0f, 640.0f, 480.0f,
                          (GXColor){0, 0, 0, 255});
    if (g_vramTiling.manager.backingValid[GLES_VRAM_TILE_LEFT])
        GlesVramDrawTextureRect(
            &g_vramTiling.backing[GLES_VRAM_TILE_LEFT], 640, 480,
            0.0f, 0.0f, 400.0f, 480.0f);
    if (g_vramTiling.manager.backingValid[GLES_VRAM_TILE_RIGHT])
        GlesVramDrawTextureRect(
            &g_vramTiling.backing[GLES_VRAM_TILE_RIGHT], 640, 480,
            400.0f, 0.0f, 640.0f, 480.0f);
#ifdef GLES_VRAM_LR_TILING_S3_TEST
    GlesVramDrawS3Diagnostics();
#endif
    RestoreDispCopyInfo();
    glInvalidateGXState();
}

#ifdef GLES_VRAM_LR_TILING_S2_TEST
int GlesVramTilingS2VisualTestReady(void)
{
    return g_vramTiling.s2VisualTestReady;
}

void GlesVramTilingS2SetVisualTestReady(void)
{
    g_vramTiling.s2VisualTestReady = 1;
}
#endif

#ifdef GLES_VRAM_LR_TILING_S3_EXPERIMENT
int GlesVramTilingS3PrepareCpuWrite(unsigned short *cpuVram,
                                    const GlesVramRect *rect)
{
    GlesVramRectPiece pieces[GLES_VRAM_MAX_WRAP_PIECES];
    uint16_t coverage[GLES_VRAM_BLOCKS_Y][GLES_VRAM_BLOCKS_X];
    unsigned char resolve[GLES_VRAM_BLOCKS_Y][GLES_VRAM_BLOCKS_X];
    uint64_t captured[GLES_VRAM_BLOCKS_Y][GLES_VRAM_BLOCKS_X];
    unsigned char tileNeeded[2] = {0, 0};
    int pieceCount;
    int pieceIndex;
    int blockX;
    int blockY;
    int anyResolve = 0;
    int tile;

    if (!g_vramTiling.initialized || cpuVram == NULL || rect == NULL)
        return 0;
#ifdef GLES_VRAM_LR_TILING_S3_TEST
    if (g_vramTiling.s3VisualTestReady)
        return 0;
#endif
    g_vramTiling.s3CpuWritePrepared = 0;
    pieceCount = GlesVramSplitWrappedRect(rect, pieces);
    if (pieceCount <= 0)
        return 0;
    memset(coverage, 0, sizeof(coverage));
    memset(resolve, 0, sizeof(resolve));
    memset(captured, 0, sizeof(captured));

    for (pieceIndex = 0; pieceIndex < pieceCount; pieceIndex++)
    {
        const GlesVramRect *piece = &pieces[pieceIndex].rect;
        int bx0 = piece->x / GLES_VRAM_BLOCK_WIDTH;
        int bx1 = (piece->x + piece->width - 1) /
                  GLES_VRAM_BLOCK_WIDTH;
        int by0 = piece->y / GLES_VRAM_BLOCK_HEIGHT;
        int by1 = (piece->y + piece->height - 1) /
                  GLES_VRAM_BLOCK_HEIGHT;
        for (blockY = by0; blockY <= by1; blockY++)
        {
            for (blockX = bx0; blockX <= bx1; blockX++)
            {
                int x0 = blockX * GLES_VRAM_BLOCK_WIDTH;
                int y0 = blockY * GLES_VRAM_BLOCK_HEIGHT;
                int x1 = x0 + GLES_VRAM_BLOCK_WIDTH;
                int y1 = y0 + GLES_VRAM_BLOCK_HEIGHT;
                int ix0 = piece->x > x0 ? piece->x : x0;
                int iy0 = piece->y > y0 ? piece->y : y0;
                int ix1 = piece->x + piece->width < x1 ?
                          piece->x + piece->width : x1;
                int iy1 = piece->y + piece->height < y1 ?
                          piece->y + piece->height : y1;
                coverage[blockY][blockX] = (uint16_t)(
                    coverage[blockY][blockX] +
                    (ix1 - ix0) * (iy1 - iy0));
            }
        }
    }

    for (blockY = 0; blockY < GLES_VRAM_BLOCKS_Y; blockY++)
    {
        for (blockX = 0; blockX < GLES_VRAM_BLOCKS_X; blockX++)
        {
            GlesVramBlockState *block;
            int cpuCurrent;
            if (coverage[blockY][blockX] == 0 ||
                coverage[blockY][blockX] ==
                    GLES_VRAM_BLOCK_WIDTH * GLES_VRAM_BLOCK_HEIGHT)
                continue;
            block = &g_vramTiling.manager.block[blockY][blockX];
            cpuCurrent = (block->flags & GLES_VRAM_BLOCK_CPU_VALID) &&
                         block->cpuSequence >= block->gpuSequence;
            if (cpuCurrent)
                continue;
            if ((block->flags & GLES_VRAM_BLOCK_GPU_VALID) == 0)
                return 0;
            resolve[blockY][blockX] = 1;
            captured[blockY][blockX] = block->gpuSequence;
            tile = blockX * GLES_VRAM_BLOCK_WIDTH >=
                   GLES_VRAM_RIGHT_X ?
                   GLES_VRAM_TILE_RIGHT : GLES_VRAM_TILE_LEFT;
            tileNeeded[tile] = 1;
            anyResolve = 1;
        }
    }
    if (!anyResolve)
    {
        g_vramTiling.s3CpuWritePrepared = 1;
        return 1;
    }

    tile = g_vramTiling.manager.activeTile;
    if (tile != GLES_VRAM_TILE_NONE && tileNeeded[tile] &&
        g_vramTiling.manager.efbDirty)
    {
        if (GlesVramManagerPersistActive(&g_vramTiling.manager) != tile)
            return 0;
        GlesVramSaveTileToBacking((GlesVramTileId)tile);
    }
    for (tile = 0; tile < 2; tile++)
    {
        if (tileNeeded[tile] &&
            !g_vramTiling.manager.backingValid[tile])
            return 0;
    }

    GX_DrawDone();
    g_vramTiling.stagingInFlight = 0;
    for (tile = 0; tile < 2; tile++)
    {
        if (tileNeeded[tile])
            DCInvalidateRange(g_vramTiling.backing[tile].pixels,
                              g_vramTiling.backing[tile].size);
    }
    for (blockY = 0; blockY < GLES_VRAM_BLOCKS_Y; blockY++)
    {
        for (blockX = 0; blockX < GLES_VRAM_BLOCKS_X; blockX++)
        {
            int x;
            int y;
            int origin;
            GlesVramBacking *backing;
            if (!resolve[blockY][blockX])
                continue;
            tile = blockX * GLES_VRAM_BLOCK_WIDTH >=
                   GLES_VRAM_RIGHT_X ?
                   GLES_VRAM_TILE_RIGHT : GLES_VRAM_TILE_LEFT;
            origin = tile == GLES_VRAM_TILE_RIGHT ?
                     GLES_VRAM_RIGHT_X : 0;
            backing = &g_vramTiling.backing[tile];
            for (y = blockY * GLES_VRAM_BLOCK_HEIGHT;
                 y < (blockY + 1) * GLES_VRAM_BLOCK_HEIGHT; y++)
            {
                for (x = blockX * GLES_VRAM_BLOCK_WIDTH;
                     x < (blockX + 1) * GLES_VRAM_BLOCK_WIDTH; x++)
                {
                    uint16_t gx = GlesVramReadGXPixel(backing,
                                                     x - origin, y);
                    GlesVramWritePsx15(cpuVram, x, y,
                                      GlesVramGXRGB5A3ToPsx15(gx));
                }
            }
            if (!GlesVramManagerResolveGpuBlock(
                    &g_vramTiling.manager, blockX, blockY,
                    captured[blockY][blockX]))
                return 0;
        }
    }
    g_vramTiling.s3CpuWritePrepared = 1;
    return 1;
}

uint64_t GlesVramTilingS3FinishCpuWrite(const GlesVramRect *rect)
{
    uint64_t sequence;

    if (!g_vramTiling.initialized || rect == NULL ||
        !g_vramTiling.s3CpuWritePrepared)
        return 0;
    g_vramTiling.s3CpuWritePrepared = 0;
    sequence = GlesVramManagerBeginWrite(&g_vramTiling.manager);
    if (sequence != 0)
        GlesVramManagerMarkCpuWrite(&g_vramTiling.manager,
                                    rect, sequence);
    return sequence;
}

int GlesVramTilingS3RectNeedsCpuUpload(const GlesVramRect *rect)
{
    return g_vramTiling.initialized &&
           GlesVramManagerRectNeedsCpuUpload(&g_vramTiling.manager, rect);
}

#ifdef GLES_VRAM_LR_TILING_S5_EXPERIMENT
static int GlesVramS5EnsureMoveScratch(size_t pixels)
{
    uint16_t *replacement;

    if (pixels <= g_vramTiling.moveScratchPixels)
        return 1;
    if (pixels > (size_t)GLES_VRAM_WIDTH * GLES_VRAM_HEIGHT)
        return 0;

    replacement = (uint16_t *)_mem2_memalign(
        32, (uint32_t)(pixels * sizeof(uint16_t)));
    if (replacement == NULL)
        return 0;
    if (g_vramTiling.moveScratch != NULL)
        _mem2_free(g_vramTiling.moveScratch);
    g_vramTiling.moveScratch = replacement;
    g_vramTiling.moveScratchPixels = pixels;
    return 1;
}

static int GlesVramS5CollectResolveBlocks(
    const GlesVramRect *rect, int partialOnly,
    uint32_t selected[(GLES_VRAM_BLOCKS_X * GLES_VRAM_BLOCKS_Y + 31) / 32],
    uint16_t indices[GLES_VRAM_BLOCKS_X * GLES_VRAM_BLOCKS_Y],
    uint64_t captured[GLES_VRAM_BLOCKS_X * GLES_VRAM_BLOCKS_Y],
    int *resolveCount, unsigned char tileNeeded[2])
{
    GlesVramRectPiece pieces[GLES_VRAM_MAX_WRAP_PIECES];
    int pieceCount;
    int pieceIndex;
    int blockX;
    int blockY;

    if (rect == NULL || resolveCount == NULL)
        return 0;
    pieceCount = GlesVramSplitWrappedRect(rect, pieces);
    if (pieceCount <= 0)
        return 0;

    for (pieceIndex = 0; pieceIndex < pieceCount; pieceIndex++)
    {
        const GlesVramRect *piece = &pieces[pieceIndex].rect;
        int bx0 = piece->x / GLES_VRAM_BLOCK_WIDTH;
        int bx1 = (piece->x + piece->width - 1) /
                  GLES_VRAM_BLOCK_WIDTH;
        int by0 = piece->y / GLES_VRAM_BLOCK_HEIGHT;
        int by1 = (piece->y + piece->height - 1) /
                  GLES_VRAM_BLOCK_HEIGHT;
        for (blockY = by0; blockY <= by1; blockY++)
        {
            for (blockX = bx0; blockX <= bx1; blockX++)
            {
                int blockIndex = blockY * GLES_VRAM_BLOCKS_X + blockX;
                GlesVramBlockState *block =
                    &g_vramTiling.manager.block[blockY][blockX];
                int cpuCurrent =
                    (block->flags & GLES_VRAM_BLOCK_CPU_VALID) != 0 &&
                    block->cpuSequence >= block->gpuSequence;
                if (partialOnly)
                {
                    int x0 = blockX * GLES_VRAM_BLOCK_WIDTH;
                    int y0 = blockY * GLES_VRAM_BLOCK_HEIGHT;
                    int x1 = x0 + GLES_VRAM_BLOCK_WIDTH;
                    int y1 = y0 + GLES_VRAM_BLOCK_HEIGHT;
                    int ix0 = piece->x > x0 ? piece->x : x0;
                    int iy0 = piece->y > y0 ? piece->y : y0;
                    int ix1 = piece->x + piece->width < x1 ?
                              piece->x + piece->width : x1;
                    int iy1 = piece->y + piece->height < y1 ?
                              piece->y + piece->height : y1;
                    if ((ix1 - ix0) * (iy1 - iy0) ==
                        GLES_VRAM_BLOCK_WIDTH * GLES_VRAM_BLOCK_HEIGHT)
                        continue;
                }
                if (cpuCurrent ||
                    (selected[blockIndex >> 5] &
                     (1u << (blockIndex & 31))) != 0)
                    continue;
                if ((block->flags & GLES_VRAM_BLOCK_GPU_VALID) == 0)
                    return 0;
                if (*resolveCount >=
                    GLES_VRAM_BLOCKS_X * GLES_VRAM_BLOCKS_Y)
                    return 0;
                selected[blockIndex >> 5] |=
                    1u << (blockIndex & 31);
                indices[*resolveCount] = (uint16_t)blockIndex;
                captured[*resolveCount] = block->gpuSequence;
                tileNeeded[blockX * GLES_VRAM_BLOCK_WIDTH >=
                           GLES_VRAM_RIGHT_X ?
                           GLES_VRAM_TILE_RIGHT : GLES_VRAM_TILE_LEFT] = 1;
                (*resolveCount)++;
            }
        }
    }
    return 1;
}

static int GlesVramS5ResolveRects(unsigned short *cpuVram,
                                 const GlesVramRect *first,
                                 int firstPartialOnly,
                                 const GlesVramRect *second,
                                 int secondPartialOnly)
{
    uint32_t selected[(GLES_VRAM_BLOCKS_X * GLES_VRAM_BLOCKS_Y + 31) / 32];
    uint16_t indices[GLES_VRAM_BLOCKS_X * GLES_VRAM_BLOCKS_Y];
    uint64_t captured[GLES_VRAM_BLOCKS_X * GLES_VRAM_BLOCKS_Y];
    unsigned char tileNeeded[2] = {0, 0};
    int resolveCount = 0;
    int resolveIndex;
    int tile;
    int waitForBacking = 0;

    if (!g_vramTiling.initialized || cpuVram == NULL || first == NULL)
        return 0;
    memset(selected, 0, sizeof(selected));
    if (!GlesVramS5CollectResolveBlocks(
            first, firstPartialOnly, selected, indices, captured,
            &resolveCount, tileNeeded) ||
        (second != NULL && !GlesVramS5CollectResolveBlocks(
            second, secondPartialOnly, selected, indices, captured,
            &resolveCount, tileNeeded)))
        return 0;
    if (resolveCount == 0)
        return 1;

    g_vramTiling.s5ResolveCalls++;
    tile = g_vramTiling.manager.activeTile;
    if (tile != GLES_VRAM_TILE_NONE && tileNeeded[tile] &&
        g_vramTiling.manager.efbDirty)
    {
        if (GlesVramManagerPersistActive(&g_vramTiling.manager) != tile)
            return 0;
        GlesVramSaveTileToBacking((GlesVramTileId)tile);
    }
    for (tile = 0; tile < 2; tile++)
    {
        if (tileNeeded[tile] &&
            !g_vramTiling.manager.backingValid[tile])
            return 0;
    }

    for (tile = 0; tile < 2; tile++)
    {
        if (tileNeeded[tile] && g_vramTiling.backingInFlight[tile])
            waitForBacking = 1;
    }
    /* Several consecutive MoveImage commands can read disjoint blocks from
     * the same saved backing without any intervening GX writer.  Wait only
     * for the first such CPU read.  Invalidate every backing completed by
     * that wait so a later command can safely read another block without a
     * redundant GX_DrawDone(). */
    if (waitForBacking)
    {
        GX_DrawDone();
        g_vramTiling.s5ResolveWaits++;
        g_vramTiling.stagingInFlight = 0;
        for (tile = 0; tile < 2; tile++)
        {
            if (g_vramTiling.backingInFlight[tile])
            {
                DCInvalidateRange(g_vramTiling.backing[tile].pixels,
                                  g_vramTiling.backing[tile].size);
                g_vramTiling.backingInFlight[tile] = 0;
            }
        }
    }

    for (resolveIndex = 0; resolveIndex < resolveCount; resolveIndex++)
    {
        int blockIndex = indices[resolveIndex];
        int blockX = blockIndex % GLES_VRAM_BLOCKS_X;
        int blockY = blockIndex / GLES_VRAM_BLOCKS_X;
        int x;
        int y;
        int origin;
        GlesVramBacking *backing;
        tile = blockX * GLES_VRAM_BLOCK_WIDTH >=
               GLES_VRAM_RIGHT_X ?
               GLES_VRAM_TILE_RIGHT : GLES_VRAM_TILE_LEFT;
        origin = tile == GLES_VRAM_TILE_RIGHT ?
                 GLES_VRAM_RIGHT_X : 0;
        backing = &g_vramTiling.backing[tile];
        for (y = blockY * GLES_VRAM_BLOCK_HEIGHT;
             y < (blockY + 1) * GLES_VRAM_BLOCK_HEIGHT; y++)
        {
            for (x = blockX * GLES_VRAM_BLOCK_WIDTH;
                 x < (blockX + 1) * GLES_VRAM_BLOCK_WIDTH; x++)
            {
                uint16_t gx = GlesVramReadGXPixel(
                    backing, x - origin, y);
                GlesVramWritePsx15(cpuVram, x, y,
                                  GlesVramGXRGB5A3ToPsx15(gx));
            }
        }
        if (!GlesVramManagerResolveGpuBlock(
                &g_vramTiling.manager, blockX, blockY,
                captured[resolveIndex]))
            return 0;
        g_vramTiling.s5ResolvedBlocks++;
    }
    return 1;
}

int GlesVramTilingS5EnsureCpuCurrent(unsigned short *cpuVram,
                                    const GlesVramRect *rect)
{
    return GlesVramS5ResolveRects(cpuVram, rect, 0, NULL, 0);
}

int GlesVramTilingS5MoveImage(unsigned short *cpuVram,
                             const GlesVramRect *source,
                             const GlesVramRect *destination,
                             uint16_t setMask)
{
    size_t pixels;
    uint64_t sequence;

    if (source == NULL || destination == NULL ||
        source->width <= 0 || source->height <= 0 ||
        source->width != destination->width ||
        source->height != destination->height)
        return 0;
    pixels = (size_t)source->width * (size_t)source->height;
    /* Allocate before any ownership transition so allocation failure can
     * safely leave the complete command to the legacy path. */
    if (!GlesVramS5EnsureMoveScratch(pixels))
        return 0;
    /* Resolve the complete source plus only partially overwritten target
     * blocks as one batch.  This gives one wait at most and avoids S3's full
     * 2048-block A0 preparation scan for every high-frequency MoveImage. */
    if (!GlesVramS5ResolveRects(cpuVram, source, 0,
                                destination, 1))
        return 0;
    if (!GlesVramCopyWrapped(cpuVram, source, destination, setMask,
                             g_vramTiling.moveScratch,
                             g_vramTiling.moveScratchPixels))
        return 0;
    sequence = GlesVramManagerBeginWrite(&g_vramTiling.manager);
    if (sequence == 0)
        return 0;
    GlesVramManagerMarkCpuWrite(&g_vramTiling.manager,
                                destination, sequence);
    g_vramTiling.s5MoveCount++;
    return 1;
}

int GlesVramTilingS5PrepareRead(unsigned short *cpuVram,
                               const GlesVramRect *rect)
{
    if (!GlesVramTilingS5EnsureCpuCurrent(cpuVram, rect))
        return 0;
    g_vramTiling.s5C0Count++;
    return 1;
}

void GlesVramTilingS5GetStats(GlesVramS5Stats *stats)
{
    if (stats == NULL)
        return;
    stats->moveCount = g_vramTiling.s5MoveCount;
    stats->c0Count = g_vramTiling.s5C0Count;
    stats->resolveCalls = g_vramTiling.s5ResolveCalls;
    stats->resolveWaits = g_vramTiling.s5ResolveWaits;
    stats->resolvedBlocks = g_vramTiling.s5ResolvedBlocks;
}
#endif

#ifdef GLES_VRAM_LR_TILING_S4_EXPERIMENT
static int GlesVramTileNeedsCpuUpload(GlesVramTileId tile)
{
    int originX = tile == GLES_VRAM_TILE_RIGHT ? GLES_VRAM_RIGHT_X : 0;
    int width = tile == GLES_VRAM_TILE_RIGHT ?
                GLES_VRAM_RIGHT_WIDTH : GLES_VRAM_LEFT_WIDTH;
    int blockX0 = originX / GLES_VRAM_BLOCK_WIDTH;
    int blockX1 = (originX + width) / GLES_VRAM_BLOCK_WIDTH;
    int blockX;
    int blockY;

    for (blockY = 0; blockY < GLES_VRAM_BLOCKS_Y; blockY++)
    {
        for (blockX = blockX0; blockX < blockX1; blockX++)
        {
            if (GlesVramManagerBlockNeedsCpuUpload(
                    &g_vramTiling.manager, blockX, blockY))
                return 1;
        }
    }
    return 0;
}

static int GlesVramPrepareDisplayTiles(const GlesVramDisplayPlan *plan)
{
    unsigned char needed[2] = {0, 0};
    int segment;
    int tile;

    if (!g_vramTiling.initialized || plan == NULL ||
        plan->segmentCount <= 0)
        return 0;
    for (segment = 0; segment < plan->segmentCount; segment++)
    {
        tile = plan->segment[segment].tile;
        if (tile != GLES_VRAM_TILE_LEFT &&
            tile != GLES_VRAM_TILE_RIGHT)
            return 0;
        needed[tile] = 1;
    }

    /* Persist the work EFB before it is reused as the presentation EFB.  A
     * tile whose backing is already current does not need to be restored just
     * to be saved again on every present. */
    GlesVramTilingS2SaveAndDiscard();
    for (tile = 0; tile < 2; tile++)
    {
        if (needed[tile] &&
            (!g_vramTiling.manager.backingValid[tile] ||
             GlesVramTileNeedsCpuUpload((GlesVramTileId)tile)) &&
            !GlesVramTilingSelectTile((GlesVramTileId)tile))
            return 0;
    }
    GlesVramTilingS2SaveAndDiscard();
    for (tile = 0; tile < 2; tile++)
    {
        if (needed[tile] && !g_vramTiling.manager.backingValid[tile])
            return 0;
    }
    return 1;
}

static void GlesVramDrawDisplayPlan(const GlesVramDisplayPlan *plan,
                                    const GlesVramRect *target,
                                    int targetWidth, int targetHeight)
{
    int segment;

    for (segment = 0; segment < plan->segmentCount; segment++)
    {
        const GlesVramDisplaySegment *part = &plan->segment[segment];
        float x0 = (float)target->x +
            (float)part->outputRect.x * (float)target->width /
            (float)plan->displayRect.width;
        float y0 = (float)target->y +
            (float)part->outputRect.y * (float)target->height /
            (float)plan->displayRect.height;
        float x1 = (float)target->x +
            (float)(part->outputRect.x + part->outputRect.width) *
            (float)target->width / (float)plan->displayRect.width;
        float y1 = (float)target->y +
            (float)(part->outputRect.y + part->outputRect.height) *
            (float)target->height / (float)plan->displayRect.height;
        GlesVramDrawTextureSubRect(
            &g_vramTiling.backing[part->tile], targetWidth, targetHeight,
            &part->sourceRect, x0, y0, x1, y1);
    }
}

#ifdef GLES_VRAM_LR_TILING_S7_PIPELINE_DIAG
static uint16_t GlesVramS7DiagReadPlanPixel(
    const GlesVramDisplayPlan *plan, int outputX, int outputY,
    int *segmentIndex)
{
    int segment;

    for (segment = 0; segment < plan->segmentCount; segment++)
    {
        const GlesVramDisplaySegment *part = &plan->segment[segment];
        if (outputX >= part->outputRect.x &&
            outputX < part->outputRect.x + part->outputRect.width &&
            outputY >= part->outputRect.y &&
            outputY < part->outputRect.y + part->outputRect.height)
        {
            if (segmentIndex != NULL)
                *segmentIndex = segment;
            return GlesVramReadGXPixel(
                &g_vramTiling.backing[part->tile],
                part->sourceRect.x + outputX - part->outputRect.x,
                part->sourceRect.y + outputY - part->outputRect.y);
        }
    }
    if (segmentIndex != NULL)
        *segmentIndex = -1;
    return GlesVramPsx15ToGXRGB5A3(0);
}

static void GlesVramS7DiagCountOwners(const GlesVramDisplayPlan *plan,
                                      GlesVramS7AnimStats *stats)
{
    uint32_t selected[(GLES_VRAM_BLOCKS_X * GLES_VRAM_BLOCKS_Y + 31) / 32];
    int segment;
    int blockX;
    int blockY;

    memset(selected, 0, sizeof(selected));
    for (segment = 0; segment < plan->segmentCount; segment++)
    {
        const GlesVramDisplaySegment *part = &plan->segment[segment];
        int origin = part->tile == GLES_VRAM_TILE_RIGHT ?
                     GLES_VRAM_RIGHT_X : 0;
        int x0 = origin + part->sourceRect.x;
        int x1 = x0 + part->sourceRect.width - 1;
        int y0 = part->sourceRect.y;
        int y1 = y0 + part->sourceRect.height - 1;

        for (blockY = y0 / GLES_VRAM_BLOCK_HEIGHT;
             blockY <= y1 / GLES_VRAM_BLOCK_HEIGHT; blockY++)
        {
            for (blockX = x0 / GLES_VRAM_BLOCK_WIDTH;
                 blockX <= x1 / GLES_VRAM_BLOCK_WIDTH; blockX++)
            {
                int index = blockY * GLES_VRAM_BLOCKS_X + blockX;
                GlesVramBlockState *block;
                int cpuCurrent;
                int gpuCurrent;
                if (selected[index >> 5] & (1u << (index & 31)))
                    continue;
                selected[index >> 5] |= 1u << (index & 31);
                block = &g_vramTiling.manager.block[blockY][blockX];
                cpuCurrent =
                    (block->flags & GLES_VRAM_BLOCK_CPU_VALID) != 0 &&
                    block->cpuSequence >= block->gpuSequence;
                gpuCurrent =
                    (block->flags & GLES_VRAM_BLOCK_GPU_VALID) != 0 &&
                    block->gpuSequence >= block->cpuSequence;
                if (cpuCurrent && gpuCurrent)
                    stats->ownerEqual++;
                else if (cpuCurrent)
                    stats->ownerCpuOnly++;
                else if (gpuCurrent)
                    stats->ownerGpuOnly++;
                else
                    stats->ownerInvalid++;
                if (GlesVramManagerBlockNeedsCpuUpload(
                        &g_vramTiling.manager, blockX, blockY))
                    stats->ownerNeedsUpload++;
            }
        }
    }
}

/* Capture after the real presentation draw has been queued.  The wait is
 * deliberately after all source sampling, so diagnostics cannot repair a
 * copy-to-texture ordering bug before it is observed on the EFB. */
static void GlesVramS7DiagCapturePipeline(
    const GlesVramDisplayPlan *plan, int targetWidth, int targetHeight)
{
    GlesVramS7AnimStats *stats = &g_vramTiling.s7AnimStats;
    unsigned char usedTile[2] = {0, 0};
    int segment;
    int x;
    int y;

    GX_SetCopyFilter(GX_FALSE, NULL, GX_FALSE, NULL);
    GX_SetTexCopySrc(0, 0, (uint16_t)targetWidth,
                    (uint16_t)targetHeight);
    GX_SetTexCopyDst((uint16_t)targetWidth, (uint16_t)targetHeight,
                     GX_TF_RGB5A3, GX_FALSE);
    GX_CopyTex(MEM_K0_TO_K1(g_vramTiling.diagEfb.pixels), GX_FALSE);
    g_vramTiling.gpuCommandsIssued = 1;
    GX_DrawDone();
    g_vramTiling.stagingInFlight = 0;

    DCInvalidateRange(g_vramTiling.diagEfb.pixels,
                      g_vramTiling.diagEfb.size);
    for (segment = 0; segment < plan->segmentCount; segment++)
        usedTile[plan->segment[segment].tile] = 1;
    for (segment = 0; segment < 2; segment++)
    {
        if (!usedTile[segment])
            continue;
        DCInvalidateRange(g_vramTiling.backing[segment].pixels,
                          g_vramTiling.backing[segment].size);
        g_vramTiling.backingInFlight[segment] = 0;
    }

    stats->sourceHash = 2166136261u;
    stats->cpuHash = 2166136261u;
    stats->efbHash = 2166136261u;
    stats->segmentCount = (uint32_t)plan->segmentCount;
    for (segment = 0; segment < plan->segmentCount; segment++)
        stats->segmentHash[segment] = 2166136261u;
    for (segment = 0; segment < 4; segment++)
        stats->efbQuadrantHash[segment] = 2166136261u;

    GlesVramS7DiagCountOwners(plan, stats);
    for (y = 0; y < plan->displayRect.height; y++)
    {
        for (x = 0; x < plan->displayRect.width; x++)
        {
            int sourceSegment;
            uint16_t gx = GlesVramS7DiagReadPlanPixel(
                plan, x, y, &sourceSegment);
            uint16_t psx = GlesVramGXRGB5A3ToPsx15(gx);
            stats->sourceHash = GlesVramS7AnimHashPixel(
                stats->sourceHash, psx);
            if (psx & 0x7fffu)
                stats->sourceNonBlack++;
            if (sourceSegment >= 0)
            {
                const GlesVramDisplaySegment *part =
                    &plan->segment[sourceSegment];
                int absoluteX = (part->tile == GLES_VRAM_TILE_RIGHT ?
                                 GLES_VRAM_RIGHT_X : 0) +
                    part->sourceRect.x + x - part->outputRect.x;
                int absoluteY = part->sourceRect.y +
                    y - part->outputRect.y;
                const GlesVramBlockState *block =
                    &g_vramTiling.manager.block[
                        absoluteY / GLES_VRAM_BLOCK_HEIGHT]
                        [absoluteX / GLES_VRAM_BLOCK_WIDTH];
                int cpuCurrent =
                    (block->flags & GLES_VRAM_BLOCK_CPU_VALID) != 0 &&
                    block->cpuSequence >= block->gpuSequence;
                stats->segmentHash[sourceSegment] =
                    GlesVramS7AnimHashPixel(
                        stats->segmentHash[sourceSegment], psx);
                if (psx & 0x7fffu)
                    stats->segmentNonBlack[sourceSegment]++;
                if (cpuCurrent && psxVuw != NULL)
                {
                    uint16_t cpu = GlesVramReadPsx15(
                        psxVuw, absoluteX, absoluteY);
                    stats->cpuHash = GlesVramS7AnimHashPixel(
                        stats->cpuHash, cpu);
                    stats->cpuComparable++;
                    if ((cpu & 0x7fffu) != (psx & 0x7fffu))
                    {
                        GlesVramS7AnimMergeBounds(
                            &stats->cpuMismatchBounds,
                            stats->cpuMismatch, x, y, 1, 1);
                        stats->cpuMismatch++;
                    }
                }
            }
        }
    }

    for (y = 0; y < targetHeight; y++)
    {
        int logicalY = (int)(((uint64_t)(2 * y + 1) *
                              (uint32_t)plan->displayRect.height) /
                             (uint32_t)(2 * targetHeight));
        if (logicalY >= plan->displayRect.height)
            logicalY = plan->displayRect.height - 1;
        for (x = 0; x < targetWidth; x++)
        {
            int quadrant = (y >= targetHeight / 2 ? 2 : 0) |
                           (x >= targetWidth / 2 ? 1 : 0);
            int logicalX = (int)(((uint64_t)(2 * x + 1) *
                                  (uint32_t)plan->displayRect.width) /
                                 (uint32_t)(2 * targetWidth));
            uint16_t actualGX;
            uint16_t expectedGX;
            uint16_t actual;
            uint16_t expected;
            if (logicalX >= plan->displayRect.width)
                logicalX = plan->displayRect.width - 1;
            actualGX = GlesVramReadGXPixel(
                &g_vramTiling.diagEfb, x, y);
            expectedGX = GlesVramS7DiagReadPlanPixel(
                plan, logicalX, logicalY, NULL);
            actual = GlesVramGXRGB5A3ToPsx15(actualGX);
            expected = GlesVramGXRGB5A3ToPsx15(expectedGX);
            stats->efbHash = GlesVramS7AnimHashPixel(
                stats->efbHash, actual);
            stats->efbQuadrantHash[quadrant] =
                GlesVramS7AnimHashPixel(
                    stats->efbQuadrantHash[quadrant], actual);
            if (actual & 0x7fffu)
            {
                stats->efbNonBlack++;
                stats->efbQuadrantNonBlack[quadrant]++;
            }
            if ((actual & 0x7fffu) != (expected & 0x7fffu))
            {
                GlesVramS7AnimMergeBounds(
                    &stats->efbMismatchBounds, stats->efbMismatch,
                    x, y, 1, 1);
                stats->efbMismatch++;
            }
        }
    }
    stats->pipelineValid = 1;
}
#endif

#ifdef GLES_VRAM_LR_TILING_S7_STITCHED_PRESENT_TEST
static int GlesVramDrawStitchedDisplayPlan(
    const GlesVramDisplayPlan *plan,
    int targetWidth, int targetHeight)
{
    GlesVramBacking stitched;
    GlesVramRect source;
    int segment;
    int tile;

    if (plan == NULL || plan->displayRect.width <= 0 ||
        plan->displayRect.width > GLES_VRAM_LEFT_WIDTH ||
        plan->displayRect.height <= 0 ||
        plan->displayRect.height > GLES_VRAM_HEIGHT)
        return 0;

    /* Both source backings can be outstanding GX copy destinations, while
     * staging can still be referenced by an earlier upload/presentation.
     * Complete those operations before the CPU builds one contiguous visible
     * texture.  This diagnostic path intentionally favors certainty over
     * throughput. */
    GX_DrawDone();
    g_vramTiling.stagingInFlight = 0;
    for (tile = 0; tile < 2; tile++)
    {
        if (g_vramTiling.backingInFlight[tile])
        {
            DCInvalidateRange(g_vramTiling.backing[tile].pixels,
                              g_vramTiling.backing[tile].size);
            g_vramTiling.backingInFlight[tile] = 0;
        }
    }

    /* Invalidate every source used by this frame even when it was populated
     * directly by the CPU.  The direct path flushes first, so this also gives
     * the stitcher one coherent view for CPU-written and GX-written tiles. */
    for (segment = 0; segment < plan->segmentCount; segment++)
    {
        tile = plan->segment[segment].tile;
        DCInvalidateRange(g_vramTiling.backing[tile].pixels,
                          g_vramTiling.backing[tile].size);
    }

    for (segment = 0; segment < plan->segmentCount; segment++)
    {
        const GlesVramDisplaySegment *part = &plan->segment[segment];
        const GlesVramBacking *backing = &g_vramTiling.backing[part->tile];
        int y;
        int x;

        for (y = 0; y < part->sourceRect.height; y++)
        {
            for (x = 0; x < part->sourceRect.width; x++)
            {
                uint16_t pixel = GlesVramReadGXPixel(
                    backing, part->sourceRect.x + x,
                    part->sourceRect.y + y);
                GlesVramWriteStagingPixel(
                    GLES_VRAM_LEFT_WIDTH,
                    part->outputRect.x + x,
                    part->outputRect.y + y,
                    pixel);
            }
        }
    }
    DCFlushRange(g_vramTiling.staging, g_vramTiling.stagingSize);

    memset(&stitched, 0, sizeof(stitched));
    stitched.pixels = g_vramTiling.staging;
    stitched.size = g_vramTiling.stagingSize;
    stitched.width = GLES_VRAM_LEFT_WIDTH;
    GX_InitTexObj(&stitched.texture, stitched.pixels,
                  GLES_VRAM_LEFT_WIDTH, GLES_VRAM_HEIGHT,
                  GX_TF_RGB5A3, GX_CLAMP, GX_CLAMP, GX_FALSE);
    GX_InitTexObjFilterMode(&stitched.texture, GX_NEAR, GX_NEAR);

    source.x = 0;
    source.y = 0;
    source.width = plan->displayRect.width;
    source.height = plan->displayRect.height;
    GlesVramSetupColor2D(targetWidth, targetHeight);
    GlesVramDrawColorRect(0.0f, 0.0f,
                          (float)targetWidth, (float)targetHeight,
                          (GXColor){0, 0, 0, 255});
    GlesVramDrawTextureSubRect(
        &stitched, targetWidth, targetHeight, &source,
        0.0f, 0.0f, (float)targetWidth, (float)targetHeight);
    g_vramTiling.stagingInFlight = 1;
    return 1;
}
#endif

int GlesVramTilingS4Present(const GlesVramRect *displayRect,
                            int targetWidth, int targetHeight)
{
    GlesVramDisplayPlan plan;
    GlesVramRect target;

    if (targetWidth <= 0 || targetHeight <= 0 ||
        !GlesVramBuildDisplayPlan(displayRect, &plan) ||
        !GlesVramPrepareDisplayTiles(&plan))
        return 0;

#ifdef GLES_VRAM_LR_TILING_S7_PRESENT_WAIT_TEST
    /* Diagnostic A/B barrier: GlesVramPrepareDisplayTiles() may finish by
     * queueing GX_CopyTex into a backing which this presentation immediately
     * samples.  Wait once here to distinguish a physical copy-to-texture
     * ordering hazard from a logical VRAM generation error. */
    GX_DrawDone();
#endif

#ifdef GLES_VRAM_LR_TILING_S7_STITCHED_PRESENT_TEST
    if (GlesVramDrawStitchedDisplayPlan(&plan, targetWidth, targetHeight))
    {
        RestoreDispCopyInfo();
        glInvalidateGXState();
        return 1;
    }
#endif

    target.x = 0;
    target.y = 0;
    target.width = targetWidth;
    target.height = targetHeight;
    GlesVramSetupColor2D(targetWidth, targetHeight);
    GlesVramDrawColorRect(0.0f, 0.0f,
                          (float)targetWidth, (float)targetHeight,
                          (GXColor){0, 0, 0, 255});
    GlesVramDrawDisplayPlan(&plan, &target, targetWidth, targetHeight);
#ifdef GLES_VRAM_LR_TILING_S7_PIPELINE_DIAG
    GlesVramS7DiagCapturePipeline(&plan, targetWidth, targetHeight);
#endif
    RestoreDispCopyInfo();
    glInvalidateGXState();
    return 1;
}

#ifdef GLES_VRAM_LR_TILING_S4_TEST
static void GlesVramS4WriteColorRect(const GlesVramRect *rect,
                                     uint16_t color)
{
    int x;
    int y;
    for (y = rect->y; y < rect->y + rect->height; y++)
    {
        for (x = rect->x; x < rect->x + rect->width; x++)
            GlesVramWritePsx15(psxVuw, x, y, color);
    }
}

void GlesVramTilingS4DrawVisualTest(void)
{
    GlesVramRect vagrant = {512, 0, 512, 256};
    GlesVramRect wrapped = {896, 256, 256, 256};
    GlesVramRect topTarget = {0, 0, 640, 240};
    GlesVramRect bottomTarget = {0, 240, 640, 240};
    GlesVramDisplayPlan topPlan;
    GlesVramDisplayPlan bottomPlan;

    if (!g_vramTiling.s4VisualTestReady)
    {
        GlesVramRect red = {512, 0, 128, 256};
        GlesVramRect blue = {640, 0, 384, 256};
        GlesVramRect green = {896, 256, 128, 256};
        GlesVramRect yellow = {0, 256, 128, 256};
        memset(psxVuw, 0, GLES_VRAM_WIDTH * GLES_VRAM_HEIGHT * 2);
        GlesVramS4WriteColorRect(&red, 31u);
        GlesVramS4WriteColorRect(&blue, (uint16_t)(31u << 10));
        GlesVramS4WriteColorRect(&green, (uint16_t)(31u << 5));
        GlesVramS4WriteColorRect(&yellow,
                                 (uint16_t)(31u | (31u << 5)));
        GlesVramTilingReset();
        if (!GlesVramBuildDisplayPlan(&vagrant, &topPlan) ||
            !GlesVramPrepareDisplayTiles(&topPlan))
            return;
        g_vramTiling.s4VisualTestReady = 1;
    }

    if (!GlesVramBuildDisplayPlan(&vagrant, &topPlan) ||
        !GlesVramBuildDisplayPlan(&wrapped, &bottomPlan))
        return;
    GlesVramSetupColor2D(640, 480);
    GlesVramDrawColorRect(0.0f, 0.0f, 640.0f, 480.0f,
                          (GXColor){0, 0, 0, 255});
    GlesVramDrawDisplayPlan(&topPlan, &topTarget, 640, 480);
    GlesVramDrawDisplayPlan(&bottomPlan, &bottomTarget, 640, 480);
    RestoreDispCopyInfo();
    glInvalidateGXState();
}
#endif
#endif

#ifdef GLES_VRAM_LR_TILING_S3_TEST
void GlesVramTilingS3CaptureEfbTestDiagnostics(void)
{
    static const uint16_t x[4] = {624, 632, 639, 623};
    int sample;

    GX_DrawDone();
    g_vramTiling.stagingInFlight = 0;
    for (sample = 0; sample < 4; sample++)
    {
        GXColor color;
        GX_PeekARGB(x[sample], 112, &color);
        g_vramTiling.s3EfbSamples[sample] =
            GlesVramS3DiagnosticGXColor(color);
    }
}

void GlesVramTilingS3CaptureTestDiagnostics(void)
{
    static const int psxX[4] = {624, 632, 640, 648};
    int sample;
    int y = 112;

    GX_DrawDone();
    g_vramTiling.stagingInFlight = 0;
    DCInvalidateRange(g_vramTiling.backing[GLES_VRAM_TILE_LEFT].pixels,
                      g_vramTiling.backing[GLES_VRAM_TILE_LEFT].size);
    DCInvalidateRange(g_vramTiling.backing[GLES_VRAM_TILE_RIGHT].pixels,
                      g_vramTiling.backing[GLES_VRAM_TILE_RIGHT].size);

    for (sample = 0; sample < 4; sample++)
    {
        g_vramTiling.s3CpuSamples[sample] =
            GlesVramPsx15ToGXRGB5A3(
                GlesVramReadPsx15(psxVuw, psxX[sample], y));
    }

    g_vramTiling.s3BackingSamples[0] = GlesVramReadGXPixel(
        &g_vramTiling.backing[GLES_VRAM_TILE_LEFT], 624, y);
    g_vramTiling.s3BackingSamples[1] = GlesVramReadGXPixel(
        &g_vramTiling.backing[GLES_VRAM_TILE_LEFT], 632, y);
    g_vramTiling.s3BackingSamples[2] = GlesVramReadGXPixel(
        &g_vramTiling.backing[GLES_VRAM_TILE_RIGHT], 0, y);
    g_vramTiling.s3BackingSamples[3] = GlesVramReadGXPixel(
        &g_vramTiling.backing[GLES_VRAM_TILE_RIGHT], 8, y);
    g_vramTiling.s3DiagnosticsReady = 1;
}

int GlesVramTilingS3VisualTestReady(void)
{
    return g_vramTiling.s3VisualTestReady;
}

void GlesVramTilingS3SetVisualTestReady(void)
{
    g_vramTiling.s3VisualTestReady = 1;
}
#endif
#endif
#endif

#ifdef GLES_VRAM_LR_TILING_S1_TEST
static void GlesVramDrawTestPattern(GlesVramTileId tile)
{
    int width = g_vramTiling.backing[tile].width;
    int halfWidth = width / 2;
    int halfHeight = GLES_VRAM_HEIGHT / 2;
    const GXColor *colors;
    static const GXColor leftColors[4] = {
        {255,   0,   0, 255}, {  0, 255,   0, 255},
        {255, 255,   0, 255}, {255,   0, 255, 255}
    };
    static const GXColor rightColors[4] = {
        {  0,   0, 255, 255}, {255, 255, 255, 255},
        {  0, 255, 255, 255}, { 96,  96,  96, 255}
    };

    colors = tile == GLES_VRAM_TILE_LEFT ? leftColors : rightColors;
    GlesVramSetupColor2D(width, GLES_VRAM_HEIGHT);
    GlesVramDrawColorRect(0.0f, 0.0f, (float)halfWidth,
                          (float)halfHeight, colors[0]);
    GlesVramDrawColorRect((float)halfWidth, 0.0f, (float)width,
                          (float)halfHeight, colors[1]);
    GlesVramDrawColorRect(0.0f, (float)halfHeight, (float)halfWidth,
                          (float)GLES_VRAM_HEIGHT, colors[2]);
    GlesVramDrawColorRect((float)halfWidth, (float)halfHeight,
                          (float)width, (float)GLES_VRAM_HEIGHT, colors[3]);
}

void GlesVramTilingRunS1VisualTest(void)
{
    if (!g_vramTiling.initialized)
        return;

    if (!g_vramTiling.visualTestReady)
    {
        /* Build both tile images, then make each one pass through an actual
         * restore -> save cycle before displaying their backing textures. */
        GlesVramTilingSelectTile(GLES_VRAM_TILE_LEFT);
        GlesVramDrawTestPattern(GLES_VRAM_TILE_LEFT);
        GlesVramTilingMarkActiveDirty();
        GlesVramTilingSelectTile(GLES_VRAM_TILE_RIGHT);
        GlesVramDrawTestPattern(GLES_VRAM_TILE_RIGHT);
        GlesVramTilingMarkActiveDirty();
        GlesVramTilingSelectTile(GLES_VRAM_TILE_LEFT);
        GlesVramTilingMarkActiveDirty();
        GlesVramTilingSelectTile(GLES_VRAM_TILE_RIGHT);
        GlesVramTilingMarkActiveDirty();
        GlesVramTilingSelectTile(GLES_VRAM_TILE_LEFT);
        GlesVramTilingDiscardActive();
        g_vramTiling.visualTestReady = 1;
    }

    GlesVramSetupColor2D(640, 480);
    GlesVramDrawColorRect(0.0f, 0.0f, 640.0f, 480.0f,
                          (GXColor){0, 0, 0, 255});
    GlesVramDrawTextureRect(
        &g_vramTiling.backing[GLES_VRAM_TILE_LEFT], 640, 480,
        0.0f, 0.0f, 320.0f, 480.0f);
    GlesVramDrawTextureRect(
        &g_vramTiling.backing[GLES_VRAM_TILE_RIGHT], 640, 480,
        320.0f, 0.0f, 640.0f, 480.0f);
    RestoreDispCopyInfo();
    glInvalidateGXState();
}
#endif

#endif
