#ifndef __GPU_NO_SWAP_PAGE_BACKING_H__
#define __GPU_NO_SWAP_PAGE_BACKING_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Lightweight two-page color backing for the 320x224 horizontal display
 * pair used by AUTO_FIX_NO_SWAP_BUF.  The ordinary renderer has one EFB;
 * these helpers preserve the other PS1 page while it is not resident. */
enum
{
    GLES_GPU_NO_SWAP_PAGE_FAILED = 0,
    GLES_GPU_NO_SWAP_PAGE_READY = 1,
    GLES_GPU_NO_SWAP_PAGE_NEEDS_VRAM = 2
};

int GlesGpuNoSwapPagePrepare(int targetX, int targetY,
                             int efbWidth, int efbHeight);
void GlesGpuNoSwapPageMarkDrawn(void);
void GlesGpuNoSwapPageReset(void);
void GlesGpuNoSwapPageShutdown(void);

#ifdef __cplusplus
}
#endif

#endif
