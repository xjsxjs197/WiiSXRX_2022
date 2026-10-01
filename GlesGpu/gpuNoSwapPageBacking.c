#include "gpuNoSwapPageBacking.h"

#include <gccore.h>
#include <string.h>

#include "../deps/opengx/GL/gl.h"
#include "../mem2_manager.h"

#define NO_SWAP_PAGE_COUNT 2
#define NO_SWAP_PAGE_Y 0
#define NO_SWAP_PAGE_X0 0
#define NO_SWAP_PAGE_X1 320

typedef struct GlesGpuNoSwapBackingTag
{
    void *pixels;
    uint32_t size;
    GXTexObj texture;
    unsigned char valid;
} GlesGpuNoSwapBacking;

typedef struct GlesGpuNoSwapPageStateTag
{
    GlesGpuNoSwapBacking page[NO_SWAP_PAGE_COUNT];
    int efbWidth;
    int efbHeight;
    int resident;
    unsigned char residentDirty;
    unsigned char initialized;
} GlesGpuNoSwapPageState;

static GlesGpuNoSwapPageState g_noSwapPage;

/* gpuPlugin owns the normal XFB copy setup.  Texture copies reuse those
 * registers, so restore them before returning to presentation. */
extern void RestoreDispCopyInfo(void);
/* Use OpenGX's explicit texture-cache region and invalidate its cached GX
 * state after direct register programming. */
extern GXTexRegion texCacheRegionS[8];

static int GlesGpuNoSwapPageIndex(int x, int y)
{
    if (y != NO_SWAP_PAGE_Y)
        return -1;
    if (x == NO_SWAP_PAGE_X0)
        return 0;
    if (x == NO_SWAP_PAGE_X1)
        return 1;
    return -1;
}

static void GlesGpuNoSwapNativeChannelsBegin(void)
{
    /* GX_CopyTex produces native RGB5A3.  OpenGX textures normally retain
     * PS1 BGR ordering and correct it with SWAP0, so use a private identity
     * table while restoring a captured EFB page. */
    GX_SetTevSwapModeTable(GX_TEV_SWAP1, GX_CH_RED, GX_CH_GREEN,
                           GX_CH_BLUE, GX_CH_ALPHA);
    GX_SetTevSwapMode(GX_TEVSTAGE0, GX_TEV_SWAP1, GX_TEV_SWAP1);
}

static void GlesGpuNoSwapNativeChannelsEnd(void)
{
    GX_SetTevSwapModeTable(GX_TEV_SWAP0, GX_CH_BLUE, GX_CH_GREEN,
                           GX_CH_RED, GX_CH_ALPHA);
    GX_SetTevSwapMode(GX_TEVSTAGE0, GX_TEV_SWAP0, GX_TEV_SWAP0);
}

static void GlesGpuNoSwapSetupCommon2D(void)
{
    Mtx44 projection;
    Mtx model;

    guOrtho(projection, 0.0f, (float)g_noSwapPage.efbHeight,
            0.0f, (float)g_noSwapPage.efbWidth, 0.0f, 1.0f);
    guMtxIdentity(model);
    GX_LoadProjectionMtx(projection, GX_ORTHOGRAPHIC);
    GX_LoadPosMtxImm(model, GX_PNMTX0);
    GX_SetCurrentMtx(GX_PNMTX0);
    GX_SetViewport(0.0f, 0.0f, (float)g_noSwapPage.efbWidth,
                   (float)g_noSwapPage.efbHeight, 0.0f, 1.0f);
    GX_SetScissor(0, 0, (uint32_t)g_noSwapPage.efbWidth,
                  (uint32_t)g_noSwapPage.efbHeight);
    GX_SetCullMode(GX_CULL_NONE);
    GX_SetZMode(GX_FALSE, GX_ALWAYS, GX_FALSE);
    GX_SetZCompLoc(GX_TRUE);
    GX_SetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR);
    GX_SetAlphaCompare(GX_ALWAYS, 0, GX_AOP_AND, GX_ALWAYS, 0);
    GX_SetColorUpdate(GX_ENABLE);
    GX_SetAlphaUpdate(GX_ENABLE);
}

static void GlesGpuNoSwapDrawBlack(void)
{
    GlesGpuNoSwapSetupCommon2D();
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

    GlesGpuNoSwapNativeChannelsBegin();
    GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
    GX_Position3f32(0.0f, 0.0f, 0.0f);
    GX_Color4u8(0, 0, 0, 0);
    GX_Position3f32((float)g_noSwapPage.efbWidth, 0.0f, 0.0f);
    GX_Color4u8(0, 0, 0, 0);
    GX_Position3f32((float)g_noSwapPage.efbWidth,
                    (float)g_noSwapPage.efbHeight, 0.0f);
    GX_Color4u8(0, 0, 0, 0);
    GX_Position3f32(0.0f, (float)g_noSwapPage.efbHeight, 0.0f);
    GX_Color4u8(0, 0, 0, 0);
    GX_End();
    GlesGpuNoSwapNativeChannelsEnd();
}

static void GlesGpuNoSwapRestore(int page)
{
    GlesGpuNoSwapBacking *backing = &g_noSwapPage.page[page];

    GlesGpuNoSwapSetupCommon2D();
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

    GlesGpuNoSwapNativeChannelsBegin();
    GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
    GX_Position3f32(0.0f, 0.0f, 0.0f);
    GX_TexCoord2f32(0.0f, 0.0f);
    GX_Position3f32((float)g_noSwapPage.efbWidth, 0.0f, 0.0f);
    GX_TexCoord2f32(1.0f, 0.0f);
    GX_Position3f32((float)g_noSwapPage.efbWidth,
                    (float)g_noSwapPage.efbHeight, 0.0f);
    GX_TexCoord2f32(1.0f, 1.0f);
    GX_Position3f32(0.0f, (float)g_noSwapPage.efbHeight, 0.0f);
    GX_TexCoord2f32(0.0f, 1.0f);
    GX_End();
    GlesGpuNoSwapNativeChannelsEnd();
}

static void GlesGpuNoSwapSave(int page)
{
    GlesGpuNoSwapBacking *backing = &g_noSwapPage.page[page];

    GX_SetCopyFilter(GX_FALSE, NULL, GX_FALSE, NULL);
    GX_SetTexCopySrc(0, 0, (uint16_t)g_noSwapPage.efbWidth,
                     (uint16_t)g_noSwapPage.efbHeight);
    GX_SetTexCopyDst((uint16_t)g_noSwapPage.efbWidth,
                     (uint16_t)g_noSwapPage.efbHeight,
                     GX_TF_RGB5A3, GX_FALSE);
    GX_CopyTex(MEM_K0_TO_K1(backing->pixels), GX_FALSE);
    GX_PixModeSync();
    backing->valid = 1;
    RestoreDispCopyInfo();
}

static int GlesGpuNoSwapAllocate(int width, int height)
{
    int page;
    uint32_t size;

    if (width <= 0 || height <= 0)
        return 0;
    size = GX_GetTexBufferSize((uint16_t)width, (uint16_t)height,
                               GX_TF_RGB5A3, 0, GX_FALSE);
    for (page = 0; page < NO_SWAP_PAGE_COUNT; page++)
    {
        GlesGpuNoSwapBacking *backing = &g_noSwapPage.page[page];
        backing->pixels = _mem2_memalign(32, size);
        if (backing->pixels == NULL)
        {
            GlesGpuNoSwapPageShutdown();
            return 0;
        }
        backing->size = size;
        memset(backing->pixels, 0, size);
        DCFlushRange(backing->pixels, size);
        GX_InitTexObj(&backing->texture, backing->pixels,
                      (uint16_t)width, (uint16_t)height,
                      GX_TF_RGB5A3, GX_CLAMP, GX_CLAMP, GX_FALSE);
        GX_InitTexObjFilterMode(&backing->texture, GX_NEAR, GX_NEAR);
    }
    g_noSwapPage.efbWidth = width;
    g_noSwapPage.efbHeight = height;
    g_noSwapPage.resident = -1;
    g_noSwapPage.initialized = 1;
    return 1;
}

int GlesGpuNoSwapPagePrepare(int targetX, int targetY,
                             int efbWidth, int efbHeight)
{
    int target = GlesGpuNoSwapPageIndex(targetX, targetY);
    int needsVram = 0;

    if (target < 0)
        return GLES_GPU_NO_SWAP_PAGE_FAILED;
    if (!g_noSwapPage.initialized &&
        !GlesGpuNoSwapAllocate(efbWidth, efbHeight))
        return GLES_GPU_NO_SWAP_PAGE_FAILED;
    if (g_noSwapPage.efbWidth != efbWidth ||
        g_noSwapPage.efbHeight != efbHeight)
        return GLES_GPU_NO_SWAP_PAGE_FAILED;

    if (g_noSwapPage.resident == target)
        return GLES_GPU_NO_SWAP_PAGE_READY;

    if (g_noSwapPage.resident >= 0 && g_noSwapPage.residentDirty)
        GlesGpuNoSwapSave(g_noSwapPage.resident);
    if (g_noSwapPage.page[target].valid)
        GlesGpuNoSwapRestore(target);
    else
    {
        /* Leave a deterministic fallback in the EFB, but tell the caller to
         * replace it with the complete PS1 VRAM page. */
        GlesGpuNoSwapDrawBlack();
        needsVram = 1;
    }

    g_noSwapPage.resident = target;
    g_noSwapPage.residentDirty = 0;
    glInvalidateGXState();
    return needsVram ? GLES_GPU_NO_SWAP_PAGE_NEEDS_VRAM :
                       GLES_GPU_NO_SWAP_PAGE_READY;
}

void GlesGpuNoSwapPageMarkDrawn(void)
{
    if (g_noSwapPage.initialized && g_noSwapPage.resident >= 0)
        g_noSwapPage.residentDirty = 1;
}

void GlesGpuNoSwapPageReset(void)
{
    int page;

    g_noSwapPage.resident = -1;
    g_noSwapPage.residentDirty = 0;
    for (page = 0; page < NO_SWAP_PAGE_COUNT; page++)
        g_noSwapPage.page[page].valid = 0;
}

void GlesGpuNoSwapPageShutdown(void)
{
    int page;

    for (page = 0; page < NO_SWAP_PAGE_COUNT; page++)
    {
        if (g_noSwapPage.page[page].pixels != NULL)
            _mem2_free(g_noSwapPage.page[page].pixels);
    }
    memset(&g_noSwapPage, 0, sizeof(g_noSwapPage));
    g_noSwapPage.resident = -1;
}
