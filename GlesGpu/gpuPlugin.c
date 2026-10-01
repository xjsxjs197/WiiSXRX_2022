/***************************************************************************
                           gpu.c  -  description
                             -------------------
    begin                : Sun Mar 08 2009
    copyright            : (C) 1999-2009 by Pete Bernert
    email                : BlackDove@addcom.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version. See also the license.txt file for *
 *   additional informations.                                              *
 *                                                                         *
 ***************************************************************************/

//*************************************************************************//
// History of changes:
//
// 2009/03/08 - Pete
// - generic cleanup for the Peops release
//
//*************************************************************************//

//#include "gpuStdafx.h"

//#include <mmsystem.h>
//#define _IN_GPU
#define _IN_GPU_LIB

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <gccore.h>

#include "gpuExternals.h"
#include "gpuPlugin.h"
#include "gpuDrawFootprint.h"
#include "gpuPendingUpload.h"
#include "gpuVramReadDependency.h"
#include "gpuVramCommandRect.h"
#include "gpuVramCommandTransfer.h"
#include "gpuNoSwapPageBacking.h"
//#include "gpuDraw.h"
//#include "gpuTexture.h"
//#include "gpuPrim.h"

#include "../gpu.h" // meh
#include "../gpulib/gpu.h"
#include "../SoftGPU/oldGpuFps.h"
#include "../mem2_manager.h"
#include "../Gamecube/wiiSXconfig.h"

//#include "NoPic.h"

#include "../gpulib/stdafx.h"

#include "../database.h"
#include "../Gamecube/DEBUG.h"
#include "../Gamecube/MEM2.h"

static short DrawSemiTrans=FALSE;
static short ly0,lx0,ly1,lx1,ly2,lx2,ly3,lx3;        // global psx vertex coords
static int   GlobalTextAddrX, GlobalTextAddrY, GlobalTextTP;
static long  GlobalTextABR,GlobalTextPAGE;
static BOOL  bUsingTWin=FALSE;
static unsigned short usMirror=0;                             // sprite mirror
static TWin_t         TWin;
static int   drawX,drawY,drawW,drawH;                 // offscreen drawing checkers
static int   iFakePrimBusy;
static BOOL  bIsFirstFrame=TRUE;

unsigned int  dwGPUVersion=0;
int           iGPUHeight=512;
int           iGPUHeightMask=511;
int           GlobalTextIL=0;
int           iTileCheat=0;

////////////////////////////////////////////////////////////////////////
// memory image of the PSX vram
////////////////////////////////////////////////////////////////////////

//unsigned char  *psxVSecure;
//unsigned char  *psxVub;
//signed   char  *psxVsb;
//unsigned short *psxVuw;
//unsigned short *psxVuw_eom;
//signed   short *psxVsw;
//unsigned int   *psxVul;
//signed   int   *psxVsl;

// macro for easy access to packet information
#define GPUCOMMAND(x) ((x>>24) & 0xff)

GLfloat         gl_z=0.0f;
BOOL            bNeedInterlaceUpdate=FALSE;
BOOL            bNeedRGB24Update=FALSE;

unsigned long   ulStatusControl[256];

////////////////////////////////////////////////////////////////////////
// global GPU vars
////////////////////////////////////////////////////////////////////////

static long     GPUdataRet;
static unsigned long gpuDataM[256];
static unsigned char gpuCommand = 0;
static long          gpuDataC = 0;
static long          gpuDataP = 0;
#ifdef GLES_VRAM_COMMAND_FIXES
static long          gpuDataWords = 0;
static GlesGpuPendingUploadSet g_pendingCpuUploads;
static int g_pendingCpuUploadsDisabled;
static uint32_t g_pendingCpuUploadFlushes;
static uint32_t g_pendingCpuUploadFallbacks;
static GlesVramCommandRect g_deferredUnmappedPage;
static int g_deferredUnmappedPageValid;

static int GlesGpuPendingUploadsManaged(void);
static void GlesGpuRegisterPendingCpuWrite(
    int x,int y,int width,int height);
static void GlesGpuRegisterDeferredUnmappedPage(
    int x,int y,int width,int height);
static void GlesGpuCommitPendingUploadRect(
    int x0,int y0,int x1,int y1,uint32_t mapId);
static void GlesGpuCommitPendingEfbWrite(
    int x,int y,int width,int height);
static void GlesGpuFlushPendingForDraw(
    const GlesVramCommandRect *drawRect);
static void GlesGpuFlushPendingForDisplay(void);
static void GlesGpuResolveDeferredUnmappedPageForDraw(
    const GlesVramCommandRect *drawRect);
static void GlesGpuResolveDeferredUnmappedPageForDisplay(
    int x,int y);
#endif

int             iDataWriteMode;
int             iDataReadMode;

int             lClearOnSwap;
int             lClearOnSwapColor;
//BOOL            bSkipNextFrame = FALSE;
int             iColDepth;
BOOL            bChangeRes;
BOOL            bWindowMode;

// possible psx display widths
short dispWidths[8] = {256,320,512,640,368,384,512,640};

short           imageX0,imageX1;
short           imageY0,imageY1;
BOOL            bDisplayNotSet = TRUE;
GLuint          uiScanLine=0;
//int             iUseScanLines=0;
//int             lSelectedSlot=0;
unsigned char * pGfxCardScreen=0;
int              cardTexBufSize = 0;
int             iBlurBuffer=0;
int             iScanBlend=0;
int             iRenderFVR=0;
int             iNoScreenSaver=0;
unsigned int    ulGPUInfoVals[16];
int             iRumbleVal    = 0;
int             iRumbleTime   = 0;

static unsigned char clearLargeRange = 0;
static unsigned short largeRangeX1 = 0;
static unsigned short largeRangeX2 = 0;
static unsigned short largeRangeY1 = 0;
static unsigned short largeRangeY2 = 0;

static unsigned short uploadAreaX1 = 0;
static unsigned short uploadAreaX2 = 0;
static unsigned short uploadAreaY1 = 0;
static unsigned short uploadAreaY2 = 0;

static unsigned short screenX = 0;
static unsigned short screenY = 0;
static unsigned short screenX1 = 320;
static unsigned short screenY1 = 240;
static unsigned short screenWidth = 320;
static unsigned short screenHeight = 240;
BOOL    canClearFrameBuf = FALSE;
BOOL    canShowFps = FALSE;

static BOOL    needUploadScreen = FALSE;
static BOOL    uploadedScreen = FALSE;
static BOOL    g_forceOpaqueMoveUpload = FALSE;
static BOOL    needFlipEGL = FALSE;
static unsigned short    RGB24Uploaded = 0;
static unsigned short    GPUupdateLace5Flg = 0;

// When display window / display mode has just changed, PreviousPSXDisplay may be stale.
// Skip CheckAgainstScreen() once to avoid matching the wrong previous screen area.
static BOOL    skipPreviousDisplayCheckOnce = FALSE;

#define CHECK_SCREEN_INFO() { \
    screenX = PSXDisplay.DisplayPosition.x; \
    screenY = PSXDisplay.DisplayPosition.y; \
    screenWidth = PSXDisplay.DisplayModeNew.x; \
    screenHeight = PSXDisplay.DisplayModeNew.y; \
    screenX1 = screenX + screenWidth; \
    screenY1 = screenY + screenHeight; \
}

#define CLEAR_SCREEN(x0, y0, x1, y1)  (((screenY1 - 1) <= y1) && (screenY >= y0) && ((screenX1 - 1) <= x1) && (screenX >= x0))

#define INRANGE(x1, x2, y1, y2) ((y2 <= largeRangeY2) && (y1 >= largeRangeY1) && (x2 <= largeRangeX2) && (x1 >= largeRangeX1))

static short   texChgType = 0;

/* Framebuffer feedback uses the full EFB as its texture.  Keep its coordinates
 * as floats; the normal PSX texture path intentionally stores 0..255 UVs in
 * bytes, which is not precise enough to register a 640-pixel EFB copy. */
static BOOL    gFramebufferTextureCoordsValid = FALSE;
static float   gFramebufferTextureCoords[4][2];

#ifdef DISP_DEBUG
/* Texture-diagnostic frame number.  Primitive logs emitted before flipEGL()
 * carry the number of the frame which is about to be presented. */
static unsigned int g_textureDiagFrame = 1;
static unsigned int g_textureDiagDraw = 0;
static unsigned int g_textureDiagEvent = 0;
static unsigned int g_textureDiagEfbClears = 0;
static char g_textureDiagBuffer[16384];
static unsigned int g_textureDiagBufferUsed = 0;

#if defined(GLES_VRAM_FLOW_DIAG) && defined(GLES_VRAM_COMMAND_FIXES)
typedef struct GlesVramFlowDiagFrameTag
{
    unsigned int fills;
    unsigned int loads;
    unsigned int moves;
    unsigned int gp1Maps;
    unsigned int coversActive;
    unsigned int coversPrevious;
    unsigned int coversCurrent;
    int fillX, fillY, fillW, fillH;
    unsigned int fillColor;
    int fillCurrent, fillNext;
    int fillContext, fillSubmitted;
    int fillPendingManaged, fillPendingBefore, fillPendingAfter;
    int loadX, loadY, loadW, loadH;
    int moveX0, moveY0, moveX1, moveY1, moveW, moveH;
    int moveUploaded;
    unsigned int uploadScreenCalls;
    unsigned int uploadFullCalls;
    unsigned int uploadChunks;
    int uploadPosition;
    unsigned int uploadMapId;
    int uploadDrawnBefore;
    int uploadX0, uploadY0, uploadX1, uploadY1;
    unsigned int uploadNonZeroPixels;
    unsigned int uploadSourceHash;
    unsigned int uploadTextureType;
    unsigned int stpWrites;
    int stpSet, stpCheck;
    unsigned int baseCovers;
    unsigned int baseOpcode, baseColor;
    int baseRawX0, baseRawY0, baseRawX1, baseRawY1;
    int baseVramX0, baseVramY0, baseVramX1, baseVramY1;
    int baseEfbX0, baseEfbY0, baseEfbX1, baseEfbY1;
    int baseSemi, baseAbr, baseMaskSet, baseMaskCheck;
    int baseZMillionths, baseSubmitted;
} GlesVramFlowDiagFrame;

static GlesVramFlowDiagFrame g_vramFlowDiag;
static uint32_t g_vramFlowPrevDrawCommands;
static uint32_t g_vramFlowPrevDrawPixels;
static uint32_t g_vramFlowPrevDrawActive;
static uint32_t g_vramFlowPrevDrawPrevious;
static uint32_t g_vramFlowPrevDrawCurrent;
static uint32_t g_vramFlowPrevDrawOutside;
static uint32_t g_vramFlowPrevDrawPending;
static uint32_t g_vramFlowPrevEfbSubmits;
static uint32_t g_vramFlowPrevReadCalls;
static uint32_t g_vramFlowPrevReadFast;
static uint32_t g_vramFlowPrevReadCaptures;
static uint32_t g_vramFlowPrevReadUnresolved;
static uint32_t g_vramFlowPrevPendingFlushes;
static uint32_t g_vramFlowPrevPendingFallbacks;
#endif

/* SD open/close per line noticeably disturbs audio timing.  Accumulate one
 * frame of diagnostics and issue a single file write at presentation. */
static void TextureDiagAppend(const char *line)
{
    unsigned int length;

    if (line == NULL)
        return;
    length = (unsigned int)strlen(line);
    if (length >= sizeof(g_textureDiagBuffer))
        return;
    if (g_textureDiagBufferUsed + length >= sizeof(g_textureDiagBuffer))
    {
        g_textureDiagBuffer[g_textureDiagBufferUsed] = '\0';
        writeLogFile(g_textureDiagBuffer);
        g_textureDiagBufferUsed = 0;
    }
    memcpy(g_textureDiagBuffer + g_textureDiagBufferUsed, line, length);
    g_textureDiagBufferUsed += length;
}

static void TextureDiagFlush(void)
{
    if (g_textureDiagBufferUsed == 0)
        return;
    g_textureDiagBuffer[g_textureDiagBufferUsed] = '\0';
    writeLogFile(g_textureDiagBuffer);
    g_textureDiagBufferUsed = 0;
}
#endif

static void ResetVramReadbackState(void);
static void BuildActiveMapFromDisplay(void);
static inline unsigned short ReadGXRGB5A3PixelRaw(
    const unsigned char *buf, int texWidth, int px, int py);
static inline unsigned short GXRGB5A3ToPSX15(unsigned short gx);
void RestoreDispCopyInfo(void);
extern GXRModeObj *vmode;     /*** Graphics Mode Object ***/

#include "gpuDraw.c"
#include "gpuTexture.c"
#include "gpuVramReadback.inc"
#include "gpuPrim.c"

#ifdef GLES_VRAM_COMMAND_FIXES
typedef struct GlesGpuDrawFootprintStatsTag
{
    uint32_t commands;
    uint32_t withPixels;
    uint32_t malformed;
    uint32_t discarded;
    uint32_t clipped;
    uint32_t outside;
    uint32_t polygon;
    uint32_t line;
    uint32_t rectangle;
    uint32_t activeMapHits;
    uint32_t previousDisplayHits;
    uint32_t currentDisplayHits;
    uint32_t pendingUploadHits;
} GlesGpuDrawFootprintStats;

/* M2 is observation-only. Keep counters available to a debugger without
 * emitting per-command SD log traffic or changing rendering decisions. */
static volatile GlesGpuDrawFootprintStats g_drawFootprintStats;
static void GlesGpuPrepareNoSwapPageForDraw(
 const GlesVramCommandRect *drawRect);

/* The opcode alone is not enough here.  The normal table deliberately maps
 * some nominal primitive encodings (for example 6Ch..6Fh) to primNI, and the
 * skip-frame table maps all drawing commands to primNI or parser-only
 * polyline handlers.  Observe only commands which the selected table will
 * actually submit to GX; M3 must not build barriers for skipped draws. */
static int GlesGpuSelectedHandlerMayDraw(void (*handler)(unsigned char *))
{
 return handler!=primNI &&
        handler!=primLineFSkip &&
        handler!=primLineGSkip;
}

static int GlesGpuDrawFootprintOverlaps(
 const GlesVramCommandRect *footprint,
 int x0,int y0,int x1,int y1)
{
 GlesVramCommandRect target;

 target.x=x0;
 target.y=y0;
 target.width=x1-x0;
 target.height=y1-y0;
 return GlesVramCommandRectsOverlap(footprint,&target);
}

static int GlesGpuObserveDrawFootprint(
 const void *packet,int wordCount,GlesGpuDrawFootprint *observed)
{
 GlesGpuDrawFootprintState state;
 GlesGpuDrawFootprint footprint;

 state.drawOffsetX=PSXDisplay.DrawOffset.x;
 state.drawOffsetY=PSXDisplay.DrawOffset.y;
 state.drawArea.x=PSXDisplay.DrawArea.x0;
 state.drawArea.y=PSXDisplay.DrawArea.y0;
 state.drawArea.width=PSXDisplay.DrawArea.x1-
                      PSXDisplay.DrawArea.x0+1;
 state.drawArea.height=PSXDisplay.DrawArea.y1-
                       PSXDisplay.DrawArea.y0+1;
 if(!GlesGpuPlanDrawFootprint(packet,wordCount,&state,&footprint))
  return 0;

 if(observed!=NULL)
  *observed=footprint;

 g_drawFootprintStats.commands++;
 if(footprint.kind==GLES_GPU_DRAW_FOOTPRINT_POLYGON)
  g_drawFootprintStats.polygon++;
 else if(footprint.kind==GLES_GPU_DRAW_FOOTPRINT_LINE)
  g_drawFootprintStats.line++;
 else if(footprint.kind==GLES_GPU_DRAW_FOOTPRINT_RECTANGLE)
  g_drawFootprintStats.rectangle++;
 if(footprint.malformed)
  g_drawFootprintStats.malformed++;
 if(footprint.discarded)
  g_drawFootprintStats.discarded++;
 if(footprint.clipped)
  g_drawFootprintStats.clipped++;
 if(footprint.whollyOutside)
  g_drawFootprintStats.outside++;
 if(!footprint.hasPixels)
  return 1;

#ifdef GLES_VRAM_COMMAND_FIXES
 GlesGpuPrepareNoSwapPageForDraw(&footprint.rect);
 GlesGpuResolveDeferredUnmappedPageForDraw(&footprint.rect);
#endif

 g_drawFootprintStats.withPixels++;
#if defined(DISP_DEBUG) && defined(GLES_VRAM_FLOW_DIAG)
 if(g_activeMap.map_valid &&
    footprint.rect.x<=g_activeMap.vram_x0 &&
    footprint.rect.y<=g_activeMap.vram_y0 &&
    footprint.rect.x+footprint.rect.width>=g_activeMap.vram_x1 &&
    footprint.rect.y+footprint.rect.height>=g_activeMap.vram_y1)
  g_vramFlowDiag.coversActive++;
 if(footprint.rect.x<=PreviousPSXDisplay.DisplayPosition.x &&
    footprint.rect.y<=PreviousPSXDisplay.DisplayPosition.y &&
    footprint.rect.x+footprint.rect.width>=PreviousPSXDisplay.DisplayEnd.x &&
    footprint.rect.y+footprint.rect.height>=PreviousPSXDisplay.DisplayEnd.y)
  g_vramFlowDiag.coversPrevious++;
 if(footprint.rect.x<=PSXDisplay.DisplayPosition.x &&
    footprint.rect.y<=PSXDisplay.DisplayPosition.y &&
    footprint.rect.x+footprint.rect.width>=PSXDisplay.DisplayEnd.x &&
    footprint.rect.y+footprint.rect.height>=PSXDisplay.DisplayEnd.y)
  g_vramFlowDiag.coversCurrent++;
#endif
 if(g_activeMap.map_valid &&
    GlesGpuDrawFootprintOverlaps(
     &footprint.rect,g_activeMap.vram_x0,g_activeMap.vram_y0,
     g_activeMap.vram_x1,g_activeMap.vram_y1))
  g_drawFootprintStats.activeMapHits++;
 if(GlesGpuDrawFootprintOverlaps(
     &footprint.rect,PreviousPSXDisplay.DisplayPosition.x,
     PreviousPSXDisplay.DisplayPosition.y,
     PreviousPSXDisplay.DisplayEnd.x,PreviousPSXDisplay.DisplayEnd.y))
  g_drawFootprintStats.previousDisplayHits++;
 if(GlesGpuDrawFootprintOverlaps(
     &footprint.rect,PSXDisplay.DisplayPosition.x,
     PSXDisplay.DisplayPosition.y,
     PSXDisplay.DisplayEnd.x,PSXDisplay.DisplayEnd.y))
  g_drawFootprintStats.currentDisplayHits++;
 if((bNeedUploadAfter &&
     GlesGpuDrawFootprintOverlaps(
      &footprint.rect,xrUploadArea.x0,xrUploadArea.y0,
      xrUploadArea.x1,xrUploadArea.y1)) ||
    GlesGpuPendingUploadFindOldest(
     &g_pendingCpuUploads,&footprint.rect,NULL,NULL))
  g_drawFootprintStats.pendingUploadHits++;

 GlesGpuFlushPendingForDraw(&footprint.rect);
 return 1;
}

#if defined(DISP_DEBUG) && defined(GLES_VRAM_FLOW_DIAG)
/* Log only anomalous commands and the command which supplies the full-page
 * base.  Comparing the dispatcher-level footprint with the callback count,
 * final GX vertices and ownership context distinguishes parser/cull failures
 * from previous-page mapping and presentation failures without flooding the
 * SD log with every primitive. */
static void GlesGpuLogFlowCommandResult(
 unsigned int opcode,int wordCount,
 const GlesGpuDrawFootprint *footprint,unsigned int submittedBefore)
{
 EfbDrawContext *ctx;
 unsigned int submitted;
 int coversPrevious;

 if(footprint==NULL || !footprint->hasPixels)
  return;

 submitted=g_debugDrawSubmitted-submittedBefore;
 coversPrevious=
  footprint->rect.x<=PreviousPSXDisplay.DisplayPosition.x &&
  footprint->rect.y<=PreviousPSXDisplay.DisplayPosition.y &&
  footprint->rect.x+footprint->rect.width>=
   PreviousPSXDisplay.DisplayEnd.x &&
  footprint->rect.y+footprint->rect.height>=
   PreviousPSXDisplay.DisplayEnd.y;
 if(submitted==1 &&
    (!coversPrevious || g_vramFlowDiag.coversPrevious!=1))
  return;

 ctx=TopEfbContext();
 sprintf(txtbuffer,
         "FMC frame=%u op=%02X words=%d sub=%u kind=%d "
         "fp=%d,%d,%d,%d flags=%d/%d/%d/%d coverPrev=%d "
         "ctx=%d/%d/%d/%d:m%u:%d,%d-%d,%d "
         "v=%d,%d;%d,%d;%d,%d;%d,%d "
         "disp=%d,%d prev=%d,%d active=%u:%d,%d-%d,%d "
         "area=%d,%d-%d,%d off=%d,%d\r\n",
         g_textureDiagFrame,opcode,wordCount,submitted,
         (int)footprint->kind,
         footprint->rect.x,footprint->rect.y,
         footprint->rect.width,footprint->rect.height,
         footprint->malformed,footprint->discarded,
         footprint->clipped,footprint->whollyOutside,coversPrevious,
         ctx!=NULL,ctx!=NULL ? ctx->rectSet : 0,
         ctx!=NULL ? ctx->submittedAny : 0,
         ctx!=NULL ? ctx->pendingRender : 0,
         ctx!=NULL ? ctx->mapId : 0,
         ctx!=NULL ? ctx->x0 : 0,ctx!=NULL ? ctx->y0 : 0,
         ctx!=NULL ? ctx->x1 : 0,ctx!=NULL ? ctx->y1 : 0,
         (int)vertex[0].x,(int)vertex[0].y,
         (int)vertex[1].x,(int)vertex[1].y,
         (int)vertex[2].x,(int)vertex[2].y,
         (int)vertex[3].x,(int)vertex[3].y,
         PSXDisplay.DisplayPosition.x,PSXDisplay.DisplayPosition.y,
         PreviousPSXDisplay.DisplayPosition.x,
         PreviousPSXDisplay.DisplayPosition.y,
         g_activeMap.map_id,g_activeMap.vram_x0,g_activeMap.vram_y0,
         g_activeMap.vram_x1,g_activeMap.vram_y1,
         PSXDisplay.DrawArea.x0,PSXDisplay.DrawArea.y0,
         PSXDisplay.DrawArea.x1,PSXDisplay.DrawArea.y1,
         PSXDisplay.DrawOffset.x,PSXDisplay.DrawOffset.y);
 TextureDiagAppend(txtbuffer);
}
#endif

static int GlesGpuDisplayRectMatchesActive(
 int x0,int y0,int x1,int y1)
{
 return g_activeMap.map_valid &&
        g_activeMap.vram_x0==x0 && g_activeMap.vram_y0==y0 &&
        g_activeMap.vram_x1==x1 && g_activeMap.vram_y1==y1;
}

static int GlesGpuPendingActivePosition(void)
{
 if(GlesGpuDisplayRectMatchesActive(
     PSXDisplay.DisplayPosition.x,PSXDisplay.DisplayPosition.y,
     PSXDisplay.DisplayEnd.x,PSXDisplay.DisplayEnd.y))
  return TRUE;
 if(GlesGpuDisplayRectMatchesActive(
     PreviousPSXDisplay.DisplayPosition.x,
     PreviousPSXDisplay.DisplayPosition.y,
     PreviousPSXDisplay.DisplayEnd.x,
     PreviousPSXDisplay.DisplayEnd.y))
  return FALSE;
 return -2;
}

static int GlesGpuPendingActiveRect(GlesVramCommandRect *rect)
{
 if(rect==NULL || !g_activeMap.map_valid ||
    g_activeMap.vram_x1<=g_activeMap.vram_x0 ||
    g_activeMap.vram_y1<=g_activeMap.vram_y0)
  return 0;
 rect->x=g_activeMap.vram_x0;
 rect->y=g_activeMap.vram_y0;
 rect->width=g_activeMap.vram_x1-g_activeMap.vram_x0;
 rect->height=g_activeMap.vram_y1-g_activeMap.vram_y0;
 return 1;
}

static int GlesGpuPendingUploadsManaged(void)
{
 /* These games currently redirect draws between display pages in legacy
  * handlers.  Until M5 removes those patches, keep their proven deferred
  * upload path intact instead of guessing the target map twice.
  *
  * Dino Crisis 1/2 must also remain on their legacy MoveImage upload path.
  * MoveImage calls UploadScreen(FALSE) for the previous display mapping.  A
  * generic M3 pending item has no mapping identity, and therefore survives
  * that upload when the previous map is not the active map.  Replaying the
  * stale item after the page switch erases menu text and corrupts DC1's load
  * menu.  M2/M3 Wii A/B testing confirmed this regression starts in M3. */
 return !g_pendingCpuUploadsDisabled &&
        !(dwActFixes & (AUTO_FIX_FF9 |
                        AUTO_FIX_FF7_DISPLAY_PAGE |
                        AUTO_FIX_NO_SWAP_BUF |
                        AUTO_FIX_DINO_CRISIS1 |
                        AUTO_FIX_DINO_CRISIS2));
}

static void GlesGpuGetDisplayPageRects(
 GlesVramCommandRect *currentPage,
 GlesVramCommandRect *previousPage)
{
 currentPage->x=PSXDisplay.DisplayPosition.x;
 currentPage->y=PSXDisplay.DisplayPosition.y;
 currentPage->width=PSXDisplay.DisplayEnd.x-
                    PSXDisplay.DisplayPosition.x;
 currentPage->height=PSXDisplay.DisplayEnd.y-
                     PSXDisplay.DisplayPosition.y;
 previousPage->x=PreviousPSXDisplay.DisplayPosition.x;
 previousPage->y=PreviousPSXDisplay.DisplayPosition.y;
 previousPage->width=PreviousPSXDisplay.DisplayEnd.x-
                     PreviousPSXDisplay.DisplayPosition.x;
 previousPage->height=PreviousPSXDisplay.DisplayEnd.y-
                      PreviousPSXDisplay.DisplayPosition.y;
}

static void GlesGpuRegisterDeferredUnmappedPage(
 int x,int y,int width,int height)
{
 GlesVramCommandRect writeRect;
 GlesVramCommandRect currentPage;
 GlesVramCommandRect previousPage;
 GlesVramCommandRect candidate;
 int displaySized;

 /* The general pending uploader deliberately stays disabled for legacy
  * no-swap-buffer games.  Retain only one complete, unmapped A0 page so it
  * can be materialized after E3/E4/E5 reveal its actual draw mapping. */
 if(!(dwActFixes&AUTO_FIX_NO_SWAP_BUF) || (dwActFixes&8) ||
    PSXDisplay.RGB24 || PSXDisplay.Interlaced ||
    PSXDisplay.InterlacedTest || width<=0 || height<=0)
  {
   g_deferredUnmappedPageValid=0;
   return;
  }

 writeRect.x=x;
 writeRect.y=y;
 writeRect.width=width;
 writeRect.height=height;
 GlesGpuGetDisplayPageRects(&currentPage,&previousPage);
 displaySized=
  (width==currentPage.width && height==currentPage.height) ||
  (width==previousPage.width && height==previousPage.height);
 if(!displaySized)
  return;

 if(GlesVramCommandSelectUnmappedDisplayPageWrite(
     &writeRect,&currentPage,&previousPage,&candidate))
  {
   g_deferredUnmappedPage=candidate;
   g_deferredUnmappedPageValid=1;
  }
 else
  g_deferredUnmappedPageValid=0;
}

static int GlesGpuUploadDeferredUnmappedPage(void)
{
 PSXRect_t savedUploadArea;
 int uploaded;

 if(!g_deferredUnmappedPageValid)
  return 0;

 savedUploadArea=xrUploadArea;
 xrUploadArea.x0=g_deferredUnmappedPage.x;
 xrUploadArea.y0=g_deferredUnmappedPage.y;
 xrUploadArea.x1=g_deferredUnmappedPage.x+
                 g_deferredUnmappedPage.width;
 xrUploadArea.y1=g_deferredUnmappedPage.y+
                 g_deferredUnmappedPage.height;
 g_deferredUnmappedPageValid=0;
 uploaded=UploadScreen(-1);
 xrUploadArea=savedUploadArea;
 return uploaded;
}

static void GlesGpuResolveDeferredUnmappedPageForDraw(
 const GlesVramCommandRect *drawRect)
{
 GlesVramCommandRect drawPage;
 GlesVramCommandRect currentPage;
 GlesVramCommandRect previousPage;
 GlesVramCommandRect uploadRect;

 if(!g_deferredUnmappedPageValid || drawRect==NULL ||
    !GlesVramCommandRectsOverlap(
      &g_deferredUnmappedPage,drawRect))
  return;

 drawPage.x=PSXDisplay.DrawArea.x0;
 drawPage.y=PSXDisplay.DrawArea.y0;
 drawPage.width=PSXDisplay.DrawArea.x1-
                PSXDisplay.DrawArea.x0+1;
 drawPage.height=PSXDisplay.DrawArea.y1-
                 PSXDisplay.DrawArea.y0+1;
 GlesGpuGetDisplayPageRects(&currentPage,&previousPage);
 if(GlesVramCommandSelectUnmappedFullDrawPage(
     &g_deferredUnmappedPage,&drawPage,
     PSXDisplay.DrawOffset.x,PSXDisplay.DrawOffset.y,
     &currentPage,&previousPage,&uploadRect))
  GlesGpuUploadDeferredUnmappedPage();
 else
  /* A draw touched the candidate without the exact page mapping.  It is no
   * longer safe to replay the CPU page later because that could erase it. */
  g_deferredUnmappedPageValid=0;
}

static void GlesGpuResolveDeferredUnmappedPageForDisplay(
 int x,int y)
{
 int width;
 int height;

 if(!g_deferredUnmappedPageValid)
  return;

 width=PSXDisplay.DisplayMode.x;
 height=PSXDisplay.DisplayMode.y+
        PreviousPSXDisplay.DisplayModeNew.y;
 if(g_deferredUnmappedPage.x==x &&
    g_deferredUnmappedPage.y==y &&
    g_deferredUnmappedPage.width==width &&
    g_deferredUnmappedPage.height==height)
  GlesGpuUploadDeferredUnmappedPage();
 /* A game may issue one or more intermediate GP1 display positions between
  * the A0 transfer and the page which consumes it.  Keep the candidate on a
  * mismatch; a later overlapping draw, a replacement full-page A0, RGB24 /
  * interlace transition, or GPU reset will invalidate it safely. */
}

static void GlesGpuPendingFallback(
 const GlesVramCommandRect *extraRect)
{
 int haveBounds=0;
 int x0=0,y0=0,x1=0,y1=0;
 int index;

 for(index=0;index<g_pendingCpuUploads.count;index++)
  {
   const GlesVramCommandRect *rect=
    &g_pendingCpuUploads.item[index].rect;
   int rectX1=rect->x+rect->width;
   int rectY1=rect->y+rect->height;
   if(!haveBounds)
    {x0=rect->x;y0=rect->y;x1=rectX1;y1=rectY1;haveBounds=1;}
   else
    {
     if(rect->x<x0) x0=rect->x;
     if(rect->y<y0) y0=rect->y;
     if(rectX1>x1) x1=rectX1;
     if(rectY1>y1) y1=rectY1;
    }
  }
 if(extraRect!=NULL && extraRect->width>0 && extraRect->height>0)
  {
   GlesVramCommandRectPiece piece[
    GLES_VRAM_COMMAND_MAX_WRAP_PIECES];
   int count=GlesVramCommandSplitWrappedRect(extraRect,piece);
   for(index=0;index<count;index++)
    {
     const GlesVramCommandRect *rect=&piece[index].rect;
     int rectX1=rect->x+rect->width;
     int rectY1=rect->y+rect->height;
     if(!haveBounds)
      {x0=rect->x;y0=rect->y;x1=rectX1;y1=rectY1;haveBounds=1;}
     else
      {
       if(rect->x<x0) x0=rect->x;
       if(rect->y<y0) y0=rect->y;
       if(rectX1>x1) x1=rectX1;
       if(rectY1>y1) y1=rectY1;
      }
    }
  }
 if(haveBounds)
  {
   xrUploadArea.x0=x0;
   xrUploadArea.y0=y0;
   xrUploadArea.x1=x1;
   xrUploadArea.y1=y1;
   bNeedUploadAfter=TRUE;
  }
 GlesGpuPendingUploadReset(&g_pendingCpuUploads);
 g_pendingCpuUploadsDisabled=1;
 g_pendingCpuUploadFallbacks++;
}

static void GlesGpuRegisterPendingCpuWrite(
 int x,int y,int width,int height)
{
 GlesVramCommandRect rect;

 if(!GlesGpuPendingUploadsManaged() || PSXDisplay.RGB24 ||
    PSXDisplay.InterlacedTest ||
    width<=0 || height<=0)
  return;
 rect.x=x;
 rect.y=y;
 rect.width=width;
 rect.height=height;
 if(!GlesGpuPendingUploadAddWrapped(&g_pendingCpuUploads,&rect))
  GlesGpuPendingFallback(&rect);
}

static void GlesGpuCommitPendingUploadRect(
 int x0,int y0,int x1,int y1,uint32_t mapId)
{
 GlesVramCommandRect rect;

 if(!GlesGpuPendingUploadsManaged() || PSXDisplay.RGB24 ||
    !g_activeMap.map_valid || mapId!=g_activeMap.map_id)
  return;
 rect.x=x0;
 rect.y=y0;
 rect.width=x1-x0;
 rect.height=y1-y0;
 if(rect.width<=0 || rect.height<=0)
  return;
 if(!GlesGpuPendingUploadSubtract(&g_pendingCpuUploads,&rect))
  GlesGpuPendingFallback(NULL);
}

static void GlesGpuCommitPendingEfbWrite(
 int x,int y,int width,int height)
{
 GlesVramCommandRect writeRect;
 GlesVramCommandRect activeRect;
 GlesVramCommandRect intersection;

 if(!GlesGpuPendingUploadsManaged() || width<=0 || height<=0 ||
    !GlesGpuPendingActiveRect(&activeRect))
  return;
 writeRect.x=x;
 writeRect.y=y;
 writeRect.width=width;
 writeRect.height=height;
 if(!GlesVramCommandIntersectRects(
     &writeRect,&activeRect,&intersection))
  return;
 if(!GlesGpuPendingUploadSubtract(
     &g_pendingCpuUploads,&intersection))
  GlesGpuPendingFallback(NULL);
}

static int GlesGpuFlushOnePending(const GlesVramCommandRect *query)
{
 GlesGpuPendingUploadSet after;
 GlesVramCommandRect activeRect;
 GlesVramCommandRect clippedQuery;
 GlesVramCommandRect uploadRect;
 PSXRect_t savedUploadArea;
 uint32_t oldestOrder=0;
 int itemIndex=-1;
 int index;
 int position;
 int uploaded;

 if(!GlesGpuPendingUploadsManaged() ||
    g_pendingCpuUploads.count<=0 || query==NULL ||
    !GlesGpuPendingActiveRect(&activeRect) ||
    !GlesVramCommandIntersectRects(
      query,&activeRect,&clippedQuery))
  return 0;
 position=GlesGpuPendingActivePosition();
 if(position!=TRUE && position!=FALSE)
  return 0;
 for(index=0;index<g_pendingCpuUploads.count;index++)
  {
   GlesVramCommandRect candidate;
   if(!GlesVramCommandRectsOverlap(
       &g_pendingCpuUploads.item[index].rect,&clippedQuery) ||
      !GlesVramCommandIntersectRects(
       &g_pendingCpuUploads.item[index].rect,
       &activeRect,&candidate) ||
      candidate.width<=1 || candidate.height<=1)
    continue;
   if(itemIndex<0 ||
      g_pendingCpuUploads.item[index].order<oldestOrder)
    {
     itemIndex=index;
     oldestOrder=g_pendingCpuUploads.item[index].order;
     uploadRect=candidate;
    }
  }

 /* UploadScreen intentionally cannot submit a 1-pixel-wide GX texture.
  * Leave such dependencies pending instead of falsely marking them clean. */
 if(itemIndex<0)
  return 0;

 after=g_pendingCpuUploads;
 if(!GlesGpuPendingUploadSubtract(&after,&uploadRect))
  {
   GlesGpuPendingFallback(NULL);
   return 0;
  }

 savedUploadArea=xrUploadArea;
 xrUploadArea.x0=uploadRect.x;
 xrUploadArea.y0=uploadRect.y;
 xrUploadArea.x1=uploadRect.x+uploadRect.width;
 xrUploadArea.y1=uploadRect.y+uploadRect.height;
 uploaded=UploadScreen(position);
 xrUploadArea=savedUploadArea;
 if(!uploaded)
  return 0;

 /* UploadScreen also commits this rectangle through its normal completion
  * hook.  Assigning the precomputed remainder is idempotent and guarantees
  * that a split cannot fail after pixels have already reached the EFB. */
 g_pendingCpuUploads=after;
 g_pendingCpuUploadFlushes++;
 return 1;
}

static void GlesGpuFlushPendingQuery(
 const GlesVramCommandRect *query)
{
 int guard=GLES_GPU_PENDING_UPLOAD_CAPACITY*2;

 while(guard-->0 && GlesGpuFlushOnePending(query))
  {}
}

static void GlesGpuFlushPendingForDraw(
 const GlesVramCommandRect *drawRect)
{
 GlesGpuFlushPendingQuery(drawRect);
}

static void GlesGpuFlushPendingForDisplay(void)
{
 GlesVramCommandRect activeRect;

 if(PSXDisplay.RGB24 ||
    !GlesGpuPendingActiveRect(&activeRect))
  return;
 GlesGpuFlushPendingQuery(&activeRect);
}

static void GlesGpuPrepareNoSwapPageForDraw(
 const GlesVramCommandRect *drawRect)
{
 PSXRect_t savedUploadArea;
 int prepareResult;
 int targetX;

 /* Vagrant Story's second Loading screen alternates two horizontal
  * 320x224 pages.  Unlike the normal direct renderer, its full-page
  * semitransparent pass needs the old contents of the page being drawn.
  * Keep this backing path deliberately narrower than AUTO_FIX_NO_SWAP_BUF
  * itself: other resolutions and mask/depth scenes retain the proven legacy
  * behavior. */
 if(!(dwActFixes&AUTO_FIX_NO_SWAP_BUF) || drawRect==NULL ||
    PSXDisplay.RGB24 || PSXDisplay.Interlaced ||
    PSXDisplay.DisplayMode.x!=320 || PSXDisplay.DisplayMode.y!=224 ||
    iSetMask!=0 || bCheckMask ||
    PSXDisplay.DisplayPosition.y!=0 ||
    PreviousPSXDisplay.DisplayPosition.y!=0 ||
    !((PSXDisplay.DisplayPosition.x==0 &&
       PreviousPSXDisplay.DisplayPosition.x==320) ||
      (PSXDisplay.DisplayPosition.x==320 &&
       PreviousPSXDisplay.DisplayPosition.x==0)))
  {
   GlesGpuNoSwapPageReset();
   return;
  }

 targetX=PreviousPSXDisplay.DisplayPosition.x;
 if(PSXDisplay.DrawArea.x0!=targetX ||
    PSXDisplay.DrawArea.y0!=0 ||
    PSXDisplay.DrawArea.x1!=targetX+319 ||
    PSXDisplay.DrawArea.y1!=223 ||
    PSXDisplay.DrawOffset.x!=targetX ||
    PSXDisplay.DrawOffset.y!=0 ||
    drawRect->x<targetX || drawRect->y<0 ||
    drawRect->x+drawRect->width>targetX+320 ||
    drawRect->y+drawRect->height>224)
  {
   GlesGpuNoSwapPageReset();
   return;
  }

 prepareResult=GlesGpuNoSwapPagePrepare(
  targetX,0,iResX,iResY);
 if(prepareResult==GLES_GPU_NO_SWAP_PAGE_NEEDS_VRAM)
  {
   /* The EFB still contains the preceding 512x480 title screen when this
    * double-buffered sequence begins.  Materialize the selected PS1 page
    * from VRAM before applying its incremental/subtractive draw commands.
    * Position -1 maps this explicit rectangle to the whole EFB instead of
    * depending on a display-position command which is issued afterwards. */
   savedUploadArea=xrUploadArea;
   xrUploadArea.x0=targetX;
   xrUploadArea.y0=0;
   xrUploadArea.x1=targetX+320;
   xrUploadArea.y1=224;
   UploadScreen(-1);
   xrUploadArea=savedUploadArea;
  }
}
#endif

static void flipEGL(void);
extern void (*ogx_draw_submitted_cb)(void);

////////////////////////////////////////////////////////////////////////
// stuff to make this a true PDK module
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
// snapshot funcs (saves screen to bitmap / text infos into file)
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
// save text infos to file
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
// GPU INIT... here starts it all (first func called by emu)
////////////////////////////////////////////////////////////////////////

#define VRAM_SIZE ((1024 * 512 * 2) + 4096)
#define VRAM_ALIGN 16
static uint16_t *vram_ptr_orig = NULL;
extern uint8_t globalVram[VRAM_SIZE + (VRAM_ALIGN - 1)];

long CALLBACK GL_GPUinit()
{
memset(ulStatusControl,0,256*sizeof(unsigned long));
#ifdef GLES_VRAM_COMMAND_FIXES
memset((void *)&g_drawFootprintStats,0,sizeof(g_drawFootprintStats));
gpuDataWords=0;
GlesGpuPendingUploadReset(&g_pendingCpuUploads);
GlesGpuNoSwapPageReset();
g_pendingCpuUploadsDisabled=0;
g_pendingCpuUploadFlushes=0;
g_pendingCpuUploadFallbacks=0;
g_deferredUnmappedPageValid=0;
#if defined(DISP_DEBUG) && defined(GLES_VRAM_FLOW_DIAG) && defined(GLES_VRAM_COMMAND_FIXES)
memset(&g_vramFlowDiag,0,sizeof(g_vramFlowDiag));
g_vramFlowPrevDrawCommands=0;
g_vramFlowPrevDrawPixels=0;
g_vramFlowPrevDrawActive=0;
g_vramFlowPrevDrawPrevious=0;
g_vramFlowPrevDrawCurrent=0;
g_vramFlowPrevDrawOutside=0;
g_vramFlowPrevDrawPending=0;
g_vramFlowPrevEfbSubmits=0;
g_vramFlowPrevReadCalls=0;
g_vramFlowPrevReadFast=0;
g_vramFlowPrevReadCaptures=0;
g_vramFlowPrevReadUnresolved=0;
g_vramFlowPrevPendingFlushes=0;
g_vramFlowPrevPendingFallbacks=0;
#endif
#endif

bChangeRes=FALSE;
bWindowMode=FALSE;

bKeepRatio = TRUE;
// different ways of accessing PSX VRAM

 //!!! ATTENTION !!!
 if (vram_ptr_orig == NULL)
 {
     //vram_ptr_orig = calloc(VRAM_SIZE + (VRAM_ALIGN-1), 1);
     vram_ptr_orig = (uint16_t *)&globalVram[0];
 }

psxVub = (unsigned char *)vram_ptr_orig;
//psxVsb=(signed char *)psxVub;
//psxVsw=(signed short *)psxVub;
//psxVsl=(signed long *)psxVub;
psxVuw=(unsigned short *)psxVub;
//psxVul=(unsigned long *)psxVub;

psxVuw_eom=psxVuw+1024*iGPUHeight;                    // pre-calc of end of vram

memset(vram_ptr_orig,0x00,VRAM_SIZE + (VRAM_ALIGN-1));
memset(ulGPUInfoVals,0x00,16*sizeof(unsigned long));

//InitFrameCap();                                       // init frame rate stuff

PSXDisplay.RGB24        = 0;                          // init vars
PreviousPSXDisplay.RGB24= 0;
PSXDisplay.Interlaced   = 0;
PSXDisplay.InterlacedTest=0;
PSXDisplay.DrawOffset.x = 0;
PSXDisplay.DrawOffset.y = 0;
PSXDisplay.DrawArea.x0  = 0;
PSXDisplay.DrawArea.y0  = 0;
PSXDisplay.DrawArea.x1  = 320;
PSXDisplay.DrawArea.y1  = 240;
PSXDisplay.DisplayMode.x= 320;
PSXDisplay.DisplayMode.y= 240;
PSXDisplay.Disabled     = FALSE;
PreviousPSXDisplay.Range.x0 =0;
PreviousPSXDisplay.Range.x1 =0;
PreviousPSXDisplay.Range.y0 =0;
PreviousPSXDisplay.Range.y1 =0;
PSXDisplay.Range.x0=0;
PSXDisplay.Range.x1=0;
PSXDisplay.Range.y0=0;
PSXDisplay.Range.y1=0;
PreviousPSXDisplay.DisplayPosition.x = 1;
PreviousPSXDisplay.DisplayPosition.y = 1;
PSXDisplay.DisplayPosition.x = 1;
PSXDisplay.DisplayPosition.y = 1;
PreviousPSXDisplay.DisplayModeNew.y=0;
PSXDisplay.Double=1;
GPUdataRet=0x400;

PSXDisplay.DisplayModeNew.x=0;
PSXDisplay.DisplayModeNew.y=0;

//PreviousPSXDisplay.Height = PSXDisplay.Height = 239;

iDataWriteMode = DR_NORMAL;

// Reset transfer values, to prevent mis-transfer of data
memset(&VRAMWrite,0,sizeof(VRAMLoad_t));
memset(&VRAMRead,0,sizeof(VRAMLoad_t));

// device initialised already !
//lGPUstatusRet = 0x74000000;

STATUSREG = 0x14802000;
GPUIsIdle;
GPUIsReadyForCommands;

return 0;
}


////////////////////////////////////////////////////////////////////////
// OPEN interface func: attention!
// some emus are calling this func in their main Window thread,
// but all other interface funcs (to draw stuff) in a different thread!
// that's a problem, since OGL is thread safe! Therefore we cannot
// initialize the OGL stuff right here, we simply set a "bIsFirstFrame = TRUE"
// flag, to initialize OGL on the first real draw call.
// btw, we also call this open func ourselfes, each time when the user
// is changing between fullscreen/window mode (ENTER key)
// btw part 2: in windows the plugin gets the window handle from the
// main emu, and doesn't create it's own window (if it would do it,
// some PAD or SPU plugins would not work anymore)
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
// I shot the sheriff... last function called from emu
////////////////////////////////////////////////////////////////////////

long CALLBACK GL_GPUshutdown()
{
 //if(psxVSecure) free(psxVSecure);                      // kill emulated vram memory
 //psxVSecure=0;

 if (pGfxCardScreen)
      {
          _mem2_free(pGfxCardScreen);
          pGfxCardScreen = 0;
      }

 vram_ptr_orig = NULL;

 return 0;
}

////////////////////////////////////////////////////////////////////////
// paint it black: simple func to clean up optical border garbage
////////////////////////////////////////////////////////////////////////

void PaintBlackBorders(void)
{
// short s;
// glDisable(GL_SCISSOR_TEST); glError();
// if(bTexEnabled) {glDisable(GL_TEXTURE_2D);bTexEnabled=FALSE;} glError();
// if(bOldSmoothShaded) {glShadeModel(GL_FLAT);bOldSmoothShaded=FALSE;} glError();
// if(bBlendEnable)     {glDisable(GL_BLEND);bBlendEnable=FALSE;} glError();
// glDisable(GL_ALPHA_TEST); glError();
//
// glEnable(GL_ALPHA_TEST); glError();
// glEnable(GL_SCISSOR_TEST); glError();

}

////////////////////////////////////////////////////////////////////////
// helper to draw scanlines
////////////////////////////////////////////////////////////////////////

//__inline void XPRIMdrawTexturedQuad(OGLVertex* vertex1, OGLVertex* vertex2,
//                                    OGLVertex* vertex3, OGLVertex* vertex4)
//{
//
//}

////////////////////////////////////////////////////////////////////////
// scanlines
////////////////////////////////////////////////////////////////////////

void SetScanLines(void)
{
}

////////////////////////////////////////////////////////////////////////
// blur, babe, blur (heavy performance hit for a so-so fullscreen effect)
////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////
// Update display (swap buffers)... called in interlaced mode on
// every emulated vsync, otherwise whenever the displayed screen region
// has been changed
////////////////////////////////////////////////////////////////////////

int iLastRGB24=0;                                      // special vars for checking when to skip two display updates
int iSkipTwo=0;

void GPUvSinc(void){
updateDisplayGl();
}

void updateDisplayGl(void)                               // UPDATE DISPLAY
{
BOOL bBlur=FALSE;


bFakeFrontBuffer=FALSE;
bRenderFrontBuffer=FALSE;

#ifdef GLES_VRAM_COMMAND_FIXES
/* CPU-new pixels must reach the active display map before borders, debug
 * overlays and presentation content can contaminate the EFB. */
GlesGpuFlushPendingForDisplay();
#endif

//if(iRenderFVR)                                        // frame buffer read fix mode still active?
// {
//  iRenderFVR--;                                       // -> if some frames in a row without read access: turn off mode
//  if(!iRenderFVR) bFullVRam=FALSE;
// }

if(iLastRGB24 && iLastRGB24!=PSXDisplay.RGB24+1)      // (mdec) garbage check
 {
  iSkipTwo=2;                                         // -> skip two frames to avoid garbage if color mode changes
 }
iLastRGB24=0;

if(PSXDisplay.RGB24)// && !bNeedUploadAfter)          // (mdec) upload wanted?
 {
      PrepareFullScreenUpload(-1);
      UploadScreen(PSXDisplay.Interlaced);                // -> upload whole screen from psx vram
  bNeedUploadTest=FALSE;
  bNeedInterlaceUpdate=FALSE;
  bNeedUploadAfter=FALSE;
  bNeedRGB24Update=FALSE;
 }
else
if(bNeedInterlaceUpdate)                              // smaller upload?
 {
     #ifdef DISP_DEBUG
     //sprintf(txtbuffer, "updateDisplayGl_2 %d %d %d %d %d %d %d %d %d\r\n", PSXDisplay.Disabled, lClearOnSwap, iZBufferDepth, PSXDisplay.Interlaced, bNeedRGB24Update, xrUploadArea.x0, xrUploadArea.x1, xrUploadArea.y0, xrUploadArea.y1);
     //writeLogFile(txtbuffer);
     #endif // DISP_DEBUG
  bNeedInterlaceUpdate=FALSE;
  xrUploadArea=xrUploadAreaIL;                        // -> upload this rect
  UploadScreen(TRUE);
 }

if (dwActFixes & AUTO_FIX_FF9) bCheckFF9G4(NULL);                 // special game fix for FF9

if(PreviousPSXDisplay.Range.x0||                      // paint black borders around display area, if needed
   PreviousPSXDisplay.Range.y0)
 PaintBlackBorders();

if(PSXDisplay.Disabled)                               // display disabled?
 {
  // moved here
  glDisable(GL_SCISSOR_TEST); glError();
  glClearColor2(0,0,0,128); glError();                 // -> clear whole backbuffer
  glClear(uiBufferBits); glError();
  glEnable(GL_SCISSOR_TEST); glError();
  gl_z=0.0f;
  bDisplayNotSet = TRUE;
  #ifdef DISP_DEBUG
  sprintf(txtbuffer, "updateDisplayGl Disabled\r\n");
  //DEBUG_print(txtbuffer, DBG_CDR1);
  writeLogFile(txtbuffer);
  #endif // DISP_DEBUG

  //gc_vout_disabled();
  //return;
 }

if(iSkipTwo)                                          // we are in skipping mood?
 {
  iSkipTwo--;
  iDrawnSomething=0;                                  // -> simply lie about something drawn
 }

//if(iBlurBuffer && !bSkipNextFrame)                    // "blur display" activated?
// {BlurBackBuffer();bBlur=TRUE;}                       // -> blur it

// if(iUseScanLines) SetScanLines();                     // "scan lines" activated? do it

// if(usCursorActive) ShowGunCursor();                   // "gun cursor" wanted? show 'em

//if(dwActFixes&128)                                    // special FPS limitation mode?
// {
//  if(bUseFrameLimit) PCFrameCap();                    // -> ok, do it
////   if(bUseFrameSkip || ulKeybits&KEY_SHOWFPS)
//   PCcalcfps();
// }

// if(gTexPicName) DisplayPic();                         // some gpu info picture active? display it

// if(bSnapShot) DoSnapShot();                           // snapshot key pressed? cheeeese :)

// if(ulKeybits&KEY_SHOWFPS)                             // wanna see FPS?
 {
//   sprintf(szDispBuf,"%06.1f",fps_cur);
//   DisplayText();                                      // -> show it
 }

//----------------------------------------------------//
// main buffer swapping (well, or skip it)

if(UseFrameSkip)                                     // frame skipping active ?
 {
  if(!bSkipNextFrame)
   {
    if(iDrawnSomething)     flipEGL();
   }
//    if((fps_skip < fFrameRateHz) && !(bSkipNextFrame))
//     {bSkipNextFrame = TRUE; fps_skip=fFrameRateHz;}
//    else bSkipNextFrame = FALSE;

 }
else                                                  // no skip ?
 {
  if(iDrawnSomething)  flipEGL();
 }

iDrawnSomething=0;

//----------------------------------------------------//

//if(lClearOnSwap)                                      // clear buffer after swap?
// {
//     #ifdef DISP_DEBUG
//     sprintf(txtbuffer, "updateDisplayGl lClearOnSwap\r\n");
//     //DEBUG_print(txtbuffer, DBG_CDR1);
//     writeLogFile(txtbuffer);
//     #endif // DISP_DEBUG
//
//  unsigned char g,b,r;
//
//  if(bDisplayNotSet)                                  // -> set new vals
//   SetOGLDisplaySettings(1);
//
//  // lClearOnSwapColor (BGR)
//  g=((unsigned char)GREEN(lClearOnSwapColor));      // -> get col
//  b=((unsigned char)BLUE(lClearOnSwapColor));
//  r=((unsigned char)RED(lClearOnSwapColor));
//  glDisable(GL_SCISSOR_TEST); glError();
//  glClearColor2(r,g,b,128); glError();                 // -> clear
//  glClear(uiBufferBits); glError();
//  glEnable(GL_SCISSOR_TEST); glError();
//  lClearOnSwap=0;                                     // -> done
// }
//else
// {
////  if(bBlur) UnBlurBackBuffer();                       // unblur buff, if blurred before
//
//  if(iZBufferDepth)                                   // clear zbuffer as well (if activated)
//   {
//       #ifdef DISP_DEBUG
//     sprintf(txtbuffer, "Not lClearOnSwap\r\n");
//     //DEBUG_print(txtbuffer, DBG_CDR1);
//     writeLogFile(txtbuffer);
//     #endif // DISP_DEBUG
//
//    //glDisable(GL_SCISSOR_TEST); glError();
//    //glClear(GL_DEPTH_BUFFER_BIT); glError();
//    //glEnable(GL_SCISSOR_TEST); glError();
//   }
// }

gl_z=0.0f;

//----------------------------------------------------//
// additional uploads immediatly after swapping

if(bNeedUploadAfter)                                  // upload wanted?
 {
  bNeedUploadAfter=FALSE;
  bNeedUploadTest=FALSE;
      #ifdef DISP_DEBUG
      sprintf(txtbuffer, "bNeedUploadAfter %d %d %d %d\r\n", xrUploadArea.x0, xrUploadArea.x1, xrUploadArea.y0, xrUploadArea.y1);
      //DEBUG_print(txtbuffer, DBG_CDR2);
      writeLogFile(txtbuffer);
      #endif // DISP_DEBUG
  UploadScreen(-1);                                   // -> upload
 }

if(bNeedUploadTest)
 {
  bNeedUploadTest=FALSE;
  if(PSXDisplay.InterlacedTest &&
     //iOffscreenDrawing>2 &&
     PreviousPSXDisplay.DisplayPosition.x==PSXDisplay.DisplayPosition.x &&
     PreviousPSXDisplay.DisplayEnd.x==PSXDisplay.DisplayEnd.x &&
     PreviousPSXDisplay.DisplayPosition.y==PSXDisplay.DisplayPosition.y &&
     PreviousPSXDisplay.DisplayEnd.y==PSXDisplay.DisplayEnd.y)
   {
       #ifdef DISP_DEBUG
       sprintf(txtbuffer, "bNeedUploadTest %d %d %d %d\r\n", xrUploadArea.x0, xrUploadArea.x1, xrUploadArea.y0, xrUploadArea.y1);
       //DEBUG_print(txtbuffer, DBG_CDR2);
       writeLogFile(txtbuffer);
       #endif // DISP_DEBUG

    PrepareFullScreenUpload(TRUE);
    UploadScreen(TRUE);
   }
 }

//----------------------------------------------------//
// rumbling (main emu pad effect)

//if(iRumbleTime)                                       // shake screen by modifying view port
// {
//  int i1=0,i2=0,i3=0,i4=0;
//
//  iRumbleTime--;
//  if(iRumbleTime)
//   {
//    i1=((rand()*iRumbleVal)/RAND_MAX)-(iRumbleVal/2);
//    i2=((rand()*iRumbleVal)/RAND_MAX)-(iRumbleVal/2);
//    i3=((rand()*iRumbleVal)/RAND_MAX)-(iRumbleVal/2);
//    i4=((rand()*iRumbleVal)/RAND_MAX)-(iRumbleVal/2);
//   }
//
//  #ifdef DISP_DEBUG
//       sprintf(txtbuffer, "iRumbleTime\r\n");
//       writeLogFile(txtbuffer);
//       #endif // DISP_DEBUG
//  glViewport(rRatioRect.left+i1,
//             iResY-(rRatioRect.top+rRatioRect.bottom)+i2,
//             rRatioRect.right+i3,
//             rRatioRect.bottom+i4); glError();
// }

//----------------------------------------------------//



// if(ulKeybits&KEY_RESETTEXSTORE) ResetStuff();         // reset on gpu mode changes? do it before next frame is filled
}

////////////////////////////////////////////////////////////////////////
// update front display: smaller update func, if something has changed
// in the frontbuffer... dirty, but hey... real men know no pain
////////////////////////////////////////////////////////////////////////

//void updateFrontDisplayGl(void)
//{
//if(PreviousPSXDisplay.Range.x0||
//   PreviousPSXDisplay.Range.y0)
// PaintBlackBorders();
//
////if(iBlurBuffer) BlurBackBuffer();
//
////if(iUseScanLines) SetScanLines();
//
//// if(usCursorActive) ShowGunCursor();
//
//bFakeFrontBuffer=FALSE;
//bRenderFrontBuffer=FALSE;
//
//// if(gTexPicName) DisplayPic();
//// if(ulKeybits&KEY_SHOWFPS) DisplayText();
//
//if(iDrawnSomething)                                   // linux:
//      flipEGL();
//
//
////if(iBlurBuffer) UnBlurBackBuffer();
//}

////////////////////////////////////////////////////////////////////////
// check if update needed
////////////////////////////////////////////////////////////////////////
void ChangeDispOffsetsXGl(void)                          // CENTER X
{
long lx,l;short sO;

if(!PSXDisplay.Range.x1) return;                      // some range given?

l=PSXDisplay.DisplayMode.x;

l*=(long)PSXDisplay.Range.x1;                         // some funky calculation
l/=2560;lx=l;l&=0xfffffff8;

if(l==PreviousPSXDisplay.Range.x1) return;            // some change?

sO=PreviousPSXDisplay.Range.x0;                       // store old

if(lx>=PSXDisplay.DisplayMode.x)                      // range bigger?
 {
  PreviousPSXDisplay.Range.x1=                        // -> take display width
   PSXDisplay.DisplayMode.x;
  PreviousPSXDisplay.Range.x0=0;                      // -> start pos is 0
 }
else                                                  // range smaller? center it
 {
  PreviousPSXDisplay.Range.x1=l;                      // -> store width (8 pixel aligned)
   PreviousPSXDisplay.Range.x0=                       // -> calc start pos
   (PSXDisplay.Range.x0-500)/8;
  if(PreviousPSXDisplay.Range.x0<0)                   // -> we don't support neg. values yet
   PreviousPSXDisplay.Range.x0=0;

  if((PreviousPSXDisplay.Range.x0+lx)>                // -> uhuu... that's too much
     PSXDisplay.DisplayMode.x)
   {
    PreviousPSXDisplay.Range.x0=                      // -> adjust start
     PSXDisplay.DisplayMode.x-lx;
    PreviousPSXDisplay.Range.x1+=lx-l;                // -> adjust width
   }
 }

if(sO!=PreviousPSXDisplay.Range.x0)                   // something changed?
 {
  bDisplayNotSet=TRUE;                                // -> recalc display stuff
 }
}

////////////////////////////////////////////////////////////////////////

void ChangeDispOffsetsYGl(void)                          // CENTER Y
{
int iT;short sO;                                      // store previous y size

if(PSXDisplay.PAL) iT=48; else iT=28;                 // different offsets on PAL/NTSC

if(PSXDisplay.Range.y0>=iT)                           // crossed the security line? :)
 {
  PreviousPSXDisplay.Range.y1=                        // -> store width
   PSXDisplay.DisplayModeNew.y;

  sO=(PSXDisplay.Range.y0-iT-4)*PSXDisplay.Double;    // -> calc offset
  if(sO<0) sO=0;

  PSXDisplay.DisplayModeNew.y+=sO;                    // -> add offset to y size, too
 }
else sO=0;                                            // else no offset

if(sO!=PreviousPSXDisplay.Range.y0)                   // something changed?
 {
  PreviousPSXDisplay.Range.y0=sO;
  bDisplayNotSet=TRUE;                                // -> recalc display stuff
 }
}

////////////////////////////////////////////////////////////////////////
// Aspect ratio of ogl screen: simply adjusting ogl view port
////////////////////////////////////////////////////////////////////////

void SetAspectRatio(void)
{
float xs,ys,s;RECT r;

if(!PSXDisplay.DisplayModeNew.x) return;
if(!PSXDisplay.DisplayModeNew.y) return;

#if 0
xs=(float)iResX/(float)PSXDisplay.DisplayModeNew.x;
ys=(float)iResY/(float)height;

s=min(xs,ys);
r.right =(int)((float)PSXDisplay.DisplayModeNew.x*s);
r.bottom=(int)((float)height*s);
if(r.right  > iResX) r.right  = iResX;
if(r.bottom > iResY) r.bottom = iResY;
if(r.right  < 1)     r.right  = 1;
if(r.bottom < 1)     r.bottom = 1;

r.left = (iResX-r.right)/2;
r.top  = (iResY-r.bottom)/2;
if(r.bottom<rRatioRect.bottom ||
   r.right <rRatioRect.right)
 {
  RECT rC;
  glClearColor2(0,0,0,128);

  if(r.right <rRatioRect.right)
   {
    rC.left=0;
    rC.top=0;
    rC.right=r.left;
    rC.bottom=iResY;
    glScissor(rC.left,rC.top,rC.right,rC.bottom);
    glClear(uiBufferBits);
    rC.left=iResX-rC.right;
    glScissor(rC.left,rC.top,rC.right,rC.bottom);

    glClear(uiBufferBits);
   }

  if(r.bottom <rRatioRect.bottom)
   {
    rC.left=0;
    rC.top=0;
    rC.right=iResX;
    rC.bottom=r.top;
    glScissor(rC.left,rC.top,rC.right,rC.bottom);

    glClear(uiBufferBits);
    rC.top=iResY-rC.bottom;
    glScissor(rC.left,rC.top,rC.right,rC.bottom);
    glClear(uiBufferBits);
   }

  bSetClip=TRUE;
  bDisplayNotSet=TRUE;
 }

rRatioRect=r;
#else
 // pcsx-rearmed hack
 //if (rearmed_get_layer_pos != NULL)
 //  rearmed_get_layer_pos(&rRatioRect.left, &rRatioRect.top, &rRatioRect.right, &rRatioRect.bottom);
  glScissor(rRatioRect.left,
           iResY-(rRatioRect.top+rRatioRect.bottom),
           rRatioRect.right,rRatioRect.bottom);
#endif

glViewport(rRatioRect.left,
           iResY-(rRatioRect.top+rRatioRect.bottom),
           rRatioRect.right,
           rRatioRect.bottom);               // init viewport
}

////////////////////////////////////////////////////////////////////////
// big ass check, if an ogl swap buffer is needed
////////////////////////////////////////////////////////////////////////

void updateDisplayIfChangedGl(void)
{
BOOL bUp;
int txStarted = 0;
GXDisplayMap proposed;

if ((PSXDisplay.DisplayMode.y == PSXDisplay.DisplayModeNew.y) &&
    (PSXDisplay.DisplayMode.x == PSXDisplay.DisplayModeNew.x))
 {
  if((PSXDisplay.RGB24      == PSXDisplay.RGB24New) &&
     (PSXDisplay.Interlaced == PSXDisplay.InterlacedNew))
     return;                                          // nothing has changed? fine, no swap buffer needed

  if (PSXDisplay.RGB24 != PSXDisplay.RGB24New)
   {
    GetProposedActiveMap(&proposed);
    proposed.rgb24 = PSXDisplay.RGB24New;
    txStarted = OnDisplayMappingWillChange(&proposed);
   }
 }
else                                                  // some res change?
 {
    GetProposedActiveMap(&proposed);
    proposed.vram_x1 = PSXDisplay.DisplayPosition.x + PSXDisplay.DisplayModeNew.x;
    proposed.vram_y1 = PSXDisplay.DisplayPosition.y + PSXDisplay.DisplayModeNew.y + PreviousPSXDisplay.DisplayModeNew.y;
    proposed.rgb24 = PSXDisplay.RGB24New;
    txStarted = OnDisplayMappingWillChange(&proposed);

    if (originalMode == ORIGINALMODE_ENABLE)
	{
		gx_vout_wait_idle();
		switchToTVMode(PSXDisplay.DisplayModeNew.x, PSXDisplay.DisplayModeNew.y, 0);
	}
    // Check if TVMode needs to be changed (240 or 480 lines)
    if (displayModeChanged)
    {
        if (originalMode == ORIGINALMODE_ENABLE && PSXDisplay.DisplayModeNew.y <= 288)
        {
            iResX = (PSXDisplay.DisplayModeNew.x <= 320) ? 640 : PSXDisplay.DisplayModeNew.x;
            iResY = 240;
        }
        else
        {
            iResX = 640;
            iResY = 480;
        }
        rRatioRect.right  = iResX;
        rRatioRect.bottom = iResY;
        displayModeChanged = 0;
    }

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity(); glError();
  glOrtho(0,PSXDisplay.DisplayModeNew.x,              // -> new psx resolution
            PSXDisplay.DisplayModeNew.y, 0, -1, 1); glError();
  #ifdef DISP_DEBUG
  sprintf(txtbuffer, "DisplayChanged glOrtho %d %d\r\n", PSXDisplay.DisplayModeNew.x, PSXDisplay.DisplayModeNew.y);
  writeLogFile(txtbuffer);
  sprintf(txtbuffer, "DisplayChanged GX_SetScissor %d %d %d %d\r\n", rRatioRect.left,
           iResY-(rRatioRect.top+rRatioRect.bottom),
           rRatioRect.right,rRatioRect.bottom);
  writeLogFile(txtbuffer);
  #endif // DISP_DEBUG
  if(bKeepRatio) SetAspectRatio();
 }

bDisplayNotSet = TRUE;                                // re-calc offsets/display area

bUp=FALSE;
if(PSXDisplay.RGB24!=PSXDisplay.RGB24New)             // clean up textures, if rgb mode change (usually mdec on/off)
 {
  PreviousPSXDisplay.RGB24=0;                         // no full 24 frame uploaded yet
  ResetTextureArea(FALSE);
  bUp=TRUE;
 }
 #ifdef DISP_DEBUG
  sprintf(txtbuffer, "updateDisplayIfChangedGl %d %d\r\n", PSXDisplay.RGB24, PSXDisplay.RGB24New);
  //DEBUG_print(txtbuffer, DBG_SPU3);
  writeLogFile(txtbuffer);
  #endif // DISP_DEBUG

PSXDisplay.RGB24         = PSXDisplay.RGB24New;       // get new infos
PSXDisplay.DisplayMode.y = PSXDisplay.DisplayModeNew.y;
PSXDisplay.DisplayMode.x = PSXDisplay.DisplayModeNew.x;
PSXDisplay.Interlaced    = PSXDisplay.InterlacedNew;

PSXDisplay.DisplayEnd.x=                              // calc new ends
 PSXDisplay.DisplayPosition.x+ PSXDisplay.DisplayMode.x;
PSXDisplay.DisplayEnd.y=
 PSXDisplay.DisplayPosition.y+ PSXDisplay.DisplayMode.y+PreviousPSXDisplay.DisplayModeNew.y;
PreviousPSXDisplay.DisplayEnd.x=
 PreviousPSXDisplay.DisplayPosition.x+ PSXDisplay.DisplayMode.x;
PreviousPSXDisplay.DisplayEnd.y=
 PreviousPSXDisplay.DisplayPosition.y+ PSXDisplay.DisplayMode.y+PreviousPSXDisplay.DisplayModeNew.y;

ChangeDispOffsetsXGl();

if(iFrameLimit==2) SetAutoFrameCap();                 // set new fps limit vals (depends on interlace)

 if(bUp)
{
    #ifdef DISP_DEBUG
    sprintf(txtbuffer, "updateDisplayIfChangedGl swap buffer\r\n");
    writeLogFile(txtbuffer);
    #endif // DISP_DEBUG
    updateDisplayGl();                              // yeah, real update (swap buffer)
}

if (txStarted)
    OnDisplayMappingChanged();
}

////////////////////////////////////////////////////////////////////////
// window mode <-> fullscreen mode (windows)
////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////
// swap update check (called by psx vsync function)
////////////////////////////////////////////////////////////////////////

//BOOL bSwapCheck(void)
//{
//static int iPosCheck=0;
//static PSXPoint_t pO;
//static PSXPoint_t pD;
//static int iDoAgain=0;
//
//if(PSXDisplay.DisplayPosition.x==pO.x &&
//   PSXDisplay.DisplayPosition.y==pO.y &&
//   PSXDisplay.DisplayEnd.x==pD.x &&
//   PSXDisplay.DisplayEnd.y==pD.y)
//     iPosCheck++;
//else iPosCheck=0;
//
//pO=PSXDisplay.DisplayPosition;
//pD=PSXDisplay.DisplayEnd;
//
//if(iPosCheck<=4) return FALSE;
//
//iPosCheck=4;
//
//if(PSXDisplay.Interlaced) return FALSE;
//
//if (bNeedInterlaceUpdate||
//    bNeedRGB24Update ||
//    bNeedUploadAfter||
//    bNeedUploadTest ||
//    iDoAgain
//   )
// {
//  iDoAgain=0;
//  if(bNeedUploadAfter)
//   iDoAgain=1;
//  if(bNeedUploadTest && PSXDisplay.InterlacedTest)
//   iDoAgain=1;
//
//  bDisplayNotSet = TRUE;
//  updateDisplayGl();
//
//  PreviousPSXDisplay.DisplayPosition.x=PSXDisplay.DisplayPosition.x;
//  PreviousPSXDisplay.DisplayPosition.y=PSXDisplay.DisplayPosition.y;
//  PreviousPSXDisplay.DisplayEnd.x=PSXDisplay.DisplayEnd.x;
//  PreviousPSXDisplay.DisplayEnd.y=PSXDisplay.DisplayEnd.y;
//  pO=PSXDisplay.DisplayPosition;
//  pD=PSXDisplay.DisplayEnd;
//
//  return TRUE;
// }
//
//return FALSE;
//}
////////////////////////////////////////////////////////////////////////
// gun cursor func: player=0-7, x=0-511, y=0-255
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
// update lace is called every VSync. Basically we limit frame rate
// here, and in interlaced mode we swap ogl display buffers.
////////////////////////////////////////////////////////////////////////

#define CALLBACK
extern void CALLBACK GPUsetframelimit(unsigned long option);
static unsigned short usFirstPos=2;

void CALLBACK GL_GPUupdateLace(void)
{
if(!(dwActFixes&AUTO_FIX_CHRONO_CROSS))
 STATUSREG^=0x80000000;                               // interlaced bit toggle, if the CC game fix is not active (see gpuReadStatus)

    static char oldframeLimit = 1;

    if ( frameLimit[0] != oldframeLimit)
        GPUsetframelimit(0);
    oldframeLimit = frameLimit[0];

//if(!(dwActFixes&128))                                 // normal frame limit func
 OldGpuCheckFrameRate();

//if(iOffscreenDrawing==4)                              // special check if high offscreen drawing is on
// {
//  if(bSwapCheck()) return;
// }

if(PSXDisplay.Interlaced)                             // interlaced mode?
 {
  if(PSXDisplay.DisplayMode.x>0 && PSXDisplay.DisplayMode.y>0)
   {
       #ifdef DISP_DEBUG
       sprintf ( txtbuffer, "GPUupdateLace1 %d %x\r\n", iDrawnSomething, RGB24Uploaded);
       writeLogFile ( txtbuffer );
       #endif // DISP_DEBUG
       updateDisplayGl();                                  // -> swap buffers (new frame)
   }
 }
else if(usFirstPos==1)                                // initial updates (after startup)
 {
     #ifdef DISP_DEBUG
    sprintf ( txtbuffer, "GPUupdateLace3\r\n");
    writeLogFile ( txtbuffer );
    #endif // DISP_DEBUG
  updateDisplayGl();
 }
 else
 {
     #ifdef DISP_DEBUG
     sprintf ( txtbuffer, "GPUupdateLace5 %x %d %d %d %x\r\n", iDrawnSomething, PSXDisplay.Interlaced, PSXDisplay.Disabled, PSXDisplay.InterlacedTest, RGB24Uploaded);
     writeLogFile ( txtbuffer );
     #endif // DISP_DEBUG
     GPUupdateLace5Flg = 0;
     if (CheckFullScreenUpload() || (needFlipEGL == TRUE && (iDrawnSomething & 0x1) == 0))
     {
         GPUupdateLace5Flg = 1;
         flipEGL();
         iDrawnSomething = 0;
     }
 }
}

////////////////////////////////////////////////////////////////////////
// process read request from GPU status register
////////////////////////////////////////////////////////////////////////

unsigned long CALLBACK GL_GPUreadStatus(void)
{
if(dwActFixes&AUTO_FIX_CHRONO_CROSS)                                 // CC game fix
 {
  static int iNumRead=0;
  if((iNumRead++)==2)
   {
    iNumRead=0;
    STATUSREG^=0x80000000;                            // interlaced bit toggle... we do it on every second read status... needed by some games (like ChronoCross)
   }
 }

if(iFakePrimBusy)                                     // 27.10.2007 - emulating some 'busy' while drawing... pfff... not perfect, but since our emulated dma is not done in an extra thread...
 {
  iFakePrimBusy--;

  if(iFakePrimBusy&1)                                 // we do a busy-idle-busy-idle sequence after/while drawing prims
   {
    GPUIsBusy;
    GPUIsNotReadyForCommands;
   }
  else
   {
    GPUIsIdle;
    GPUIsReadyForCommands;
   }
 }

return STATUSREG;
}

////////////////////////////////////////////////////////////////////////
// processes data send to GPU status register
// these are always single packet commands.
////////////////////////////////////////////////////////////////////////

void CALLBACK GL_GPUwriteStatus(unsigned long gdata)
{
unsigned long lCommand=(gdata>>24)&0xff;

if(bIsFirstFrame) GLinitialize(NULL, NULL);           // real ogl startup (needed by some emus)

ulStatusControl[lCommand]=gdata;

switch(lCommand)
 {
  //--------------------------------------------------//
  // reset gpu
  case 0x00:
   memset(ulGPUInfoVals,0x00,16*sizeof(unsigned long));
   lGPUstatusRet=0x14802000;
   PSXDisplay.Disabled=1;
   iDataWriteMode=iDataReadMode=DR_NORMAL;
   PSXDisplay.DrawOffset.x=PSXDisplay.DrawOffset.y=0;
   drawX=drawY=0;drawW=drawH=0;
   sSetMask=0;lSetMask=0;bCheckMask=FALSE;iSetMask=0;
   usMirror=0;
   GlobalTextAddrX=0;GlobalTextAddrY=0;
   GlobalTextTP=0;GlobalTextABR=0;
   PSXDisplay.RGB24=FALSE;
   PSXDisplay.Interlaced=FALSE;
   bUsingTWin = FALSE;
#ifdef GLES_VRAM_COMMAND_FIXES
   GlesGpuPendingUploadReset(&g_pendingCpuUploads);
   GlesGpuNoSwapPageReset();
   g_pendingCpuUploadsDisabled=0;
   g_deferredUnmappedPageValid=0;
   bNeedUploadAfter=FALSE;
#if defined(DISP_DEBUG) && defined(GLES_VRAM_FLOW_DIAG) && defined(GLES_VRAM_COMMAND_FIXES)
   memset(&g_vramFlowDiag,0,sizeof(g_vramFlowDiag));
#endif
#endif
   return;

  // dis/enable display
  case 0x03:
   PreviousPSXDisplay.Disabled = PSXDisplay.Disabled;
   PSXDisplay.Disabled = (gdata & 1);

   if(PSXDisplay.Disabled)
        STATUSREG|=GPUSTATUS_DISPLAYDISABLED;
   else STATUSREG&=~GPUSTATUS_DISPLAYDISABLED;

   if (iOffscreenDrawing==4 &&
        PreviousPSXDisplay.Disabled &&
       !(PSXDisplay.Disabled))
    {

     if(!PSXDisplay.RGB24)
      {
       PrepareFullScreenUpload(TRUE);
       #ifdef DISP_DEBUG
       sprintf(txtbuffer, "dis/enable display %d %d %d %d\r\n", xrUploadArea.x0, xrUploadArea.x1, xrUploadArea.y0, xrUploadArea.y1);
       //DEBUG_print(txtbuffer, DBG_CDR2);
       writeLogFile(txtbuffer);
       #endif // DISP_DEBUG
       UploadScreen(TRUE);
       updateDisplayGl();
      }
    }

   return;

  // setting transfer mode
  case 0x04:
   gdata &= 0x03;                                     // only want the lower two bits

   iDataWriteMode=iDataReadMode=DR_NORMAL;
   if(gdata==0x02) iDataWriteMode=DR_VRAMTRANSFER;
   if(gdata==0x03) iDataReadMode =DR_VRAMTRANSFER;

   STATUSREG&=~GPUSTATUS_DMABITS;                     // clear the current settings of the DMA bits
   STATUSREG|=(gdata << 29);                          // set the DMA bits according to the received data

   return;

  // setting display position
  case 0x05:
   {
    short sx=(short)(gdata & 0x3ff);
    short sy;
    GXDisplayMap proposed;
    int txStarted = 0;
#ifdef DISP_DEBUG
    short diagOldCurrentX = PSXDisplay.DisplayPosition.x;
    short diagOldCurrentY = PSXDisplay.DisplayPosition.y;
    short diagOldPreviousX = PreviousPSXDisplay.DisplayPosition.x;
    short diagOldPreviousY = PreviousPSXDisplay.DisplayPosition.y;
#endif

    if(iGPUHeight==1024)
     {
      if(dwGPUVersion==2)
           sy = (short)((gdata>>12)&0x3ff);
      else sy = (short)((gdata>>10)&0x3ff);
     }
    else sy = (short)((gdata>>10)&0x3ff);             // really: 0x1ff, but we adjust it later

    if (sy & 0x200)
     {
      sy|=0xfc00;
      PreviousPSXDisplay.DisplayModeNew.y=sy/PSXDisplay.Double;
      sy=0;
     }
    else PreviousPSXDisplay.DisplayModeNew.y=0;

    if(sx>1000) sx=0;

#ifdef GLES_VRAM_COMMAND_FIXES
    GlesGpuResolveDeferredUnmappedPageForDisplay(sx,sy);
#endif

    if(usFirstPos)
     {
      usFirstPos--;
      if(usFirstPos)
       {
        GetProposedActiveMap(&proposed);
        proposed.vram_x0 = sx;
        proposed.vram_y0 = sy;
        proposed.vram_x1 = sx + PSXDisplay.DisplayMode.x;
        proposed.vram_y1 = sy + PSXDisplay.DisplayMode.y + PreviousPSXDisplay.DisplayModeNew.y;
        txStarted = OnDisplayMappingWillChange(&proposed);

        PreviousPSXDisplay.DisplayPosition.x = sx;
        PreviousPSXDisplay.DisplayPosition.y = sy;
        PSXDisplay.DisplayPosition.x = sx;
        PSXDisplay.DisplayPosition.y = sy;

        if (txStarted)
            OnDisplayMappingChanged();
        txStarted = 0;
       }
     }

    if(dwActFixes&8)
     {
      if((!PSXDisplay.Interlaced) &&
         PreviousPSXDisplay.DisplayPosition.x == sx  &&
         PreviousPSXDisplay.DisplayPosition.y == sy)
       return;

      GetProposedActiveMap(&proposed);
      proposed.vram_x0 = PreviousPSXDisplay.DisplayPosition.x;
      proposed.vram_y0 = PreviousPSXDisplay.DisplayPosition.y;
      proposed.vram_x1 = proposed.vram_x0 + PSXDisplay.DisplayMode.x;
      proposed.vram_y1 = proposed.vram_y0 + PSXDisplay.DisplayMode.y + PreviousPSXDisplay.DisplayModeNew.y;
      txStarted = OnDisplayMappingWillChange(&proposed);

      PSXDisplay.DisplayPosition.x = PreviousPSXDisplay.DisplayPosition.x;
      PSXDisplay.DisplayPosition.y = PreviousPSXDisplay.DisplayPosition.y;
      PreviousPSXDisplay.DisplayPosition.x = sx;
      PreviousPSXDisplay.DisplayPosition.y = sy;
     }
    else
     {
      if((!PSXDisplay.Interlaced) &&
         PSXDisplay.DisplayPosition.x == sx  &&
         PSXDisplay.DisplayPosition.y == sy)
       return;
      GetProposedActiveMap(&proposed);
      proposed.vram_x0 = sx;
      proposed.vram_y0 = sy;
      proposed.vram_x1 = sx + PSXDisplay.DisplayMode.x;
      proposed.vram_y1 = sy + PSXDisplay.DisplayMode.y + PreviousPSXDisplay.DisplayModeNew.y;
      txStarted = OnDisplayMappingWillChange(&proposed);

      PreviousPSXDisplay.DisplayPosition.x = PSXDisplay.DisplayPosition.x;
      PreviousPSXDisplay.DisplayPosition.y = PSXDisplay.DisplayPosition.y;
      PSXDisplay.DisplayPosition.x = sx;
      PSXDisplay.DisplayPosition.y = sy;
     }

    PSXDisplay.DisplayEnd.x=
     PSXDisplay.DisplayPosition.x+ PSXDisplay.DisplayMode.x;
    PSXDisplay.DisplayEnd.y=
     PSXDisplay.DisplayPosition.y+ PSXDisplay.DisplayMode.y+PreviousPSXDisplay.DisplayModeNew.y;

    PreviousPSXDisplay.DisplayEnd.x=
     PreviousPSXDisplay.DisplayPosition.x+ PSXDisplay.DisplayMode.x;
    PreviousPSXDisplay.DisplayEnd.y=
     PreviousPSXDisplay.DisplayPosition.y+ PSXDisplay.DisplayMode.y+PreviousPSXDisplay.DisplayModeNew.y;

    if (txStarted)
        OnDisplayMappingChanged();

#ifdef DISP_DEBUG
#if defined(GLES_VRAM_FLOW_DIAG) && defined(GLES_VRAM_COMMAND_FIXES)
    g_vramFlowDiag.gp1Maps++;
#endif
    g_textureDiagEvent++;
    sprintf(txtbuffer,
            "TDI GP1 frame=%u event=%u req=%d,%d "
            "old=%d,%d/%d,%d new=%d,%d/%d,%d fix8=%d\r\n",
            g_textureDiagFrame, g_textureDiagEvent, sx, sy,
            diagOldCurrentX, diagOldCurrentY,
            diagOldPreviousX, diagOldPreviousY,
            PSXDisplay.DisplayPosition.x, PSXDisplay.DisplayPosition.y,
            PreviousPSXDisplay.DisplayPosition.x,
            PreviousPSXDisplay.DisplayPosition.y,
            (dwActFixes & 8) != 0);
    TextureDiagAppend(txtbuffer);
#endif

    bDisplayNotSet = TRUE;

    if (!(PSXDisplay.Interlaced))
     {
         #ifdef DISP_DEBUG
         sprintf(txtbuffer, "settingDispInfo05 %d %d %d %d\r\n", PSXDisplay.DisplayPosition.x, PSXDisplay.DisplayPosition.y, PSXDisplay.DisplayMode.x * PSXDisplay.Range.x1 / 2560, PSXDisplay.Height);
         //DEBUG_print(txtbuffer, DBG_CDR2);
         writeLogFile(txtbuffer);
         #endif // DISP_DEBUG
         CHECK_SCREEN_INFO();

         if (GPUupdateLace5Flg && (iDrawnSomething & ~0x4) == 0)
         {

         }
         else
         {
             skipPreviousDisplayCheckOnce = TRUE;
             updateDisplayGl();
         }
     }
    else
    if(PSXDisplay.InterlacedTest &&
       ((PreviousPSXDisplay.DisplayPosition.x != PSXDisplay.DisplayPosition.x)||
        (PreviousPSXDisplay.DisplayPosition.y != PSXDisplay.DisplayPosition.y)))
     PSXDisplay.InterlacedTest--;

    return;
   }

  // setting width
  case 0x06:
   {
    short oldRangeX0 = PSXDisplay.Range.x0;
    short oldRangeX1 = PSXDisplay.Range.x1;
    int txStarted = 0;

    PSXDisplay.Range.x0=gdata & 0x7ff;      //0x3ff;
    PSXDisplay.Range.x1=(gdata>>12) & 0xfff;//0x7ff;

    PSXDisplay.Range.x1-=PSXDisplay.Range.x0;

    if (oldRangeX0 != PSXDisplay.Range.x0 ||
        oldRangeX1 != PSXDisplay.Range.x1)
        txStarted = OnDisplayMappingWillChange(NULL);

    CHECK_SCREEN_INFO();
    #ifdef DISP_DEBUG
      sprintf(txtbuffer, "settingDispInfo06 width %d %d\r\n", screenWidth, screenHeight);
      writeLogFile(txtbuffer);
      #endif // DISP_DEBUG
    ChangeDispOffsetsXGl();

    if (txStarted)
        OnDisplayMappingChanged();

    return;
   }

  // setting height
  case 0x07:
   {
    int txStarted = 0;

    PreviousPSXDisplay.Height = PSXDisplay.Height;

    PSXDisplay.Range.y0=gdata & 0x3ff;
    PSXDisplay.Range.y1=(gdata>>10) & 0x3ff;

    PSXDisplay.Height = PSXDisplay.Range.y1 -
                        PSXDisplay.Range.y0 +
                        PreviousPSXDisplay.DisplayModeNew.y;

    if (PreviousPSXDisplay.Height != PSXDisplay.Height)
     {
      txStarted = OnDisplayMappingWillChange(NULL);

      PSXDisplay.DisplayModeNew.y=PSXDisplay.Height*PSXDisplay.Double;
      ChangeDispOffsetsYGl();

      #ifdef DISP_DEBUG
      sprintf(txtbuffer, "settingDispInfo07 height %d %d\r\n", screenWidth, screenHeight);
      writeLogFile(txtbuffer);
      #endif // DISP_DEBUG
      CHECK_SCREEN_INFO();

      skipPreviousDisplayCheckOnce = TRUE;
      updateDisplayIfChangedGl();

      if (txStarted)
          OnDisplayMappingChanged();
     }

    return;
   }

  // setting display infos
  case 0x08:
   {
    GXDisplayMap proposed;
    int txStarted;

    GetProposedActiveMap(&proposed);
    proposed.rgb24 = (gdata & 0x10) ? TRUE : FALSE;
    proposed.vram_x1 = PSXDisplay.DisplayPosition.x +
                       dispWidths[(gdata & 0x03) | ((gdata & 0x40) >> 4)];
    proposed.vram_y1 = PSXDisplay.DisplayPosition.y +
                       PSXDisplay.Height * ((gdata & 0x04) ? 2 : 1) +
                       PreviousPSXDisplay.DisplayModeNew.y;
    txStarted = OnDisplayMappingWillChange(&proposed);

    PSXDisplay.DisplayModeNew.x = dispWidths[(gdata & 0x03) | ((gdata & 0x40) >> 4)];

   if (gdata&0x04) PSXDisplay.Double=2;
   else            PSXDisplay.Double=1;
   PSXDisplay.DisplayModeNew.y = PSXDisplay.Height*PSXDisplay.Double;

   ChangeDispOffsetsYGl();

   PSXDisplay.PAL           = (gdata & 0x08)?TRUE:FALSE; // if 1 - PAL mode, else NTSC
   PSXDisplay.RGB24New      = (gdata & 0x10)?TRUE:FALSE; // if 1 - TrueColor
   PSXDisplay.InterlacedNew = ((gdata & 0x24) ^ 0x24)?FALSE:TRUE; // if 0 - Interlace

   STATUSREG&=~GPUSTATUS_WIDTHBITS;                   // clear the width bits

   STATUSREG|=
              (((gdata & 0x03) << 17) |
              ((gdata & 0x40) << 10));                // set the width bits

   PreviousPSXDisplay.InterlacedNew=FALSE;
   if (PSXDisplay.InterlacedNew)
    {
     if(!PSXDisplay.Interlaced)
      {
       PSXDisplay.InterlacedTest=2;
       PreviousPSXDisplay.DisplayPosition.x = PSXDisplay.DisplayPosition.x;
       PreviousPSXDisplay.DisplayPosition.y = PSXDisplay.DisplayPosition.y;
       PreviousPSXDisplay.InterlacedNew=TRUE;
      }

     STATUSREG|=GPUSTATUS_INTERLACED;
    }
   else
    {
     PSXDisplay.InterlacedTest=0;
     STATUSREG&=~GPUSTATUS_INTERLACED;
    }

   if (PSXDisplay.PAL)
        STATUSREG|=GPUSTATUS_PAL;
   else STATUSREG&=~GPUSTATUS_PAL;

   if (PSXDisplay.Double==2)
        STATUSREG|=GPUSTATUS_DOUBLEHEIGHT;
   else STATUSREG&=~GPUSTATUS_DOUBLEHEIGHT;

   if (PSXDisplay.RGB24New)
        STATUSREG|=GPUSTATUS_RGB24;
   else STATUSREG&=~GPUSTATUS_RGB24;

     CHECK_SCREEN_INFO();
     #ifdef DISP_DEBUG
     sprintf(txtbuffer, "settingDispInfo08 %d %d %d %d\r\n", PSXDisplay.DisplayPosition.x, PSXDisplay.DisplayPosition.y, screenWidth, screenHeight);
     writeLogFile(txtbuffer);
     #endif // DISP_DEBUG

   skipPreviousDisplayCheckOnce = TRUE;
   updateDisplayIfChangedGl();

   if (txStarted)
       OnDisplayMappingChanged();

   return;
   }

  //--------------------------------------------------//
  // ask about GPU version and other stuff
  case 0x10:

   gdata&=0xff;

   switch(gdata)
    {
     case 0x02:
      GPUdataRet=ulGPUInfoVals[INFO_TW];              // tw infos
      return;
     case 0x03:
      GPUdataRet=ulGPUInfoVals[INFO_DRAWSTART];       // draw start
      return;
     case 0x04:
      GPUdataRet=ulGPUInfoVals[INFO_DRAWEND];         // draw end
      return;
     case 0x05:
     case 0x06:
      GPUdataRet=ulGPUInfoVals[INFO_DRAWOFF];         // draw offset
      return;
     case 0x07:
      if(dwGPUVersion==2)
           GPUdataRet=0x01;
      else GPUdataRet=0x02;                           // gpu type
      return;
     case 0x08:
     case 0x0F:                                       // some bios addr?
      GPUdataRet=0xBFC03720;
      return;
    }
   return;
  //--------------------------------------------------//
 }
}

////////////////////////////////////////////////////////////////////////
// vram read/write helpers
////////////////////////////////////////////////////////////////////////

BOOL bNeedWriteUpload=FALSE;

#ifdef GLES_VRAM_COMMAND_FIXES
static void GlesVramCommandAdvanceTransfer(VRAMLoad_t *transfer)
{
 transfer->RowsRemaining--;
 if(transfer->RowsRemaining<=0)
  {
   transfer->RowsRemaining=transfer->Width;
   transfer->ColsRemaining--;
  }
}

static void GlesVramCommandWriteTransferPixel(unsigned short value)
{
 int row=VRAMWrite.Height-VRAMWrite.ColsRemaining;
 int column=VRAMWrite.Width-VRAMWrite.RowsRemaining;
 GlesVramCommandWritePixel(psxVuw,
                           VRAMWrite.x+column,
                           VRAMWrite.y+row,
                           value,sSetMask!=0,bCheckMask);
 GlesVramCommandAdvanceTransfer(&VRAMWrite);
}

#endif

static __inline void FinishedVRAMWrite(void)
{
#if defined(DISP_DEBUG) && defined(GLES_VRAM_FLOW_DIAG) && defined(GLES_VRAM_COMMAND_FIXES)
 g_vramFlowDiag.loads++;
 g_vramFlowDiag.loadX=VRAMWrite.x;
 g_vramFlowDiag.loadY=VRAMWrite.y;
 g_vramFlowDiag.loadW=VRAMWrite.Width;
 g_vramFlowDiag.loadH=VRAMWrite.Height;
#endif
 if (VramOwnershipTrackingEnabled())
 {
  MarkCpuVramWrite(VRAMWrite.x, VRAMWrite.y,
                   VRAMWrite.Width, VRAMWrite.Height);
#ifdef DISP_DEBUG
  if (VRAMWrite.Height >= 120)
   DebugLogVramHalf("A0Done", VRAMWrite.x, VRAMWrite.y,
                    VRAMWrite.Width, VRAMWrite.Height);
#endif
 }

#ifdef GLES_VRAM_COMMAND_FIXES
 GlesGpuRegisterPendingCpuWrite(
  VRAMWrite.x,VRAMWrite.y,VRAMWrite.Width,VRAMWrite.Height);
 GlesGpuRegisterDeferredUnmappedPage(
  VRAMWrite.x,VRAMWrite.y,VRAMWrite.Width,VRAMWrite.Height);
#endif

 if(bNeedWriteUpload)
  {
   bNeedWriteUpload=FALSE;
   CheckWriteUpdate();
  }

 // set register to NORMAL operation
 iDataWriteMode = DR_NORMAL;

 // reset transfer values, to prevent mis-transfer of data
 VRAMWrite.ColsRemaining = 0;
 VRAMWrite.RowsRemaining = 0;
}

static __inline void FinishedVRAMRead(void)
{
 g_readbackState = READBACK_IDLE;

 // set register to NORMAL operation
 iDataReadMode = DR_NORMAL;
 // reset transfer values, to prevent mis-transfer of data
 VRAMRead.x = 0;
 VRAMRead.y = 0;
 VRAMRead.Width = 0;
 VRAMRead.Height = 0;
 VRAMRead.ColsRemaining = 0;
 VRAMRead.RowsRemaining = 0;

 // indicate GPU is no longer ready for VRAM data in the STATUS REGISTER
 STATUSREG&=~GPUSTATUS_READYFORVRAM;
}

////////////////////////////////////////////////////////////////////////
// vram read check ex (reading from card's back/frontbuffer if needed...
// slow!)
////////////////////////////////////////////////////////////////////////

void CheckVRamReadEx(int x, int y, int dx, int dy)
{
    #ifdef DISP_DEBUG
    //sprintf(txtbuffer, "CheckVRamReadEx  \r\n");
    //DEBUG_print(txtbuffer, DBG_CORE2);
    #endif // DISP_DEBUG
}

////////////////////////////////////////////////////////////////////////
// vram read check (reading from card's back/frontbuffer if needed...
// slow!)
////////////////////////////////////////////////////////////////////////

// don't do GL vram read
//void CheckVRamRead(int x, int y, int dx, int dy, bool bFront)
//{
//}

void RestoreDispCopyInfo(void)
{
    float yscale = GX_GetYScaleFactor(vmode->efbHeight,vmode->xfbHeight);
    int xfbHeight = GX_SetDispCopyYScale(yscale);
    GX_SetScissor(0,0,vmode->fbWidth,vmode->efbHeight);
    GX_SetDispCopySrc(0,0,vmode->fbWidth,vmode->efbHeight);
    GX_SetDispCopyDst(vmode->fbWidth,xfbHeight);
    GX_SetCopyFilter(vmode->aa,vmode->sample_pattern,GX_TRUE,vmode->vfilter);
    GX_SetFieldMode(vmode->field_rendering,((vmode->viHeight==2*vmode->xfbHeight)?GX_ENABLE:GX_DISABLE));
}

static inline unsigned short ReadGXRGB5A3PixelRaw(const unsigned char* buf, int texWidth, int px, int py)
{
    int blocksPerRow = texWidth >> 2;
    int blockIndex   = (py >> 2) * blocksPerRow + (px >> 2);
    int blockOffset  = blockIndex << 5;
    int pixelOffset  = (((py & 3) << 2) + (px & 3)) << 1;

    const unsigned char* p = buf + blockOffset + pixelOffset;

    return (unsigned short)(((unsigned short)p[0] << 8) | (unsigned short)p[1]);
}

static inline unsigned short GXRGB5A3ToPSX15(unsigned short gx)
{
    unsigned short psx;

    if (gx & 0x8000)
    {
        unsigned short r5 = (gx >> 10) & 0x1F;
        unsigned short g5 = (gx >> 5) & 0x1F;
        unsigned short b5 = gx & 0x1F;

        /* GX RGB5A3 stores RGB from high to low bits, while PS1 VRAM
         * stores red in bits 0-4 and blue in bits 10-14. */
        psx = (unsigned short)(r5 | (g5 << 5) | (b5 << 10));
    }
    else
    {
        unsigned short r4 = (gx >> 8) & 0xF;
        unsigned short g4 = (gx >> 4) & 0xF;
        unsigned short b4 = (gx >> 0) & 0xF;

        unsigned short r5 = (r4 << 1) | (r4 >> 3);
        unsigned short g5 = (g4 << 1) | (g4 >> 3);
        unsigned short b5 = (b4 << 1) | (b4 >> 3);
        psx = (unsigned short)(r5 | (g5 << 5) | (b5 << 10));
    }

    return psx;
}

static inline void CheckVRamRead(int x, int y, int dx, int dy)
{
 if (!ReadbackEnabled()) return;
 MergeReadbackToPsxVuw(x, y, dx - x, dy - y);
}

////////////////////////////////////////////////////////////////////////
// core read from vram
////////////////////////////////////////////////////////////////////////

void CALLBACK GL_GPUreadDataMem(unsigned long * pMem, int iSize)
{
int i;

#ifdef DISP_DEBUG
static unsigned int readCallCount;
readCallCount++;
#ifdef GLES_VRAM_COMMAND_FIXES
if (readCallCount <= 4 ||
    (ReadbackEnabled() &&
     (iDataReadMode == DR_VRAMTRANSFER ||
      g_readbackState == READBACK_PENDING)))
#else
if (readCallCount <= 4 ||
    iDataReadMode == DR_VRAMTRANSFER ||
    g_readbackState == READBACK_PENDING)
#endif
 {
  sprintf(txtbuffer,
          "VRB READ call=%u size=%d mode=%d state=%d enabled=%d "
          "rect=%d,%d %dx%d\r\n",
          readCallCount, iSize, iDataReadMode, g_readbackState,
          ReadbackEnabled(), VRAMRead.x, VRAMRead.y,
          VRAMRead.Width, VRAMRead.Height);
  writeLogFile(txtbuffer);
 }
#endif

if(iDataReadMode!=DR_VRAMTRANSFER) return;

GPUIsBusy;

if (g_readbackState == READBACK_PENDING)
 {
#ifdef GLES_VRAM_COMMAND_FIXES
  GlesGpuEnsureCpuCurrent(VRAMRead.x, VRAMRead.y,
                          VRAMRead.Width, VRAMRead.Height,
                          VRAM_READ_REASON_C0);
#else
  g_lastReadMapping = ClassifyReadMapping(VRAMRead.x, VRAMRead.y,
                                          VRAMRead.Width, VRAMRead.Height);
  if (g_lastReadMapping == MAPPING_CURRENT)
   TryCaptureLiveFrame();
  else if (g_lastReadMapping == MAPPING_PREVIOUS &&
           TryCapturePreviousReadRect(VRAMRead.x, VRAMRead.y,
                                      VRAMRead.Width, VRAMRead.Height))
   g_lastCaptureResult = 3;
  else
   g_lastCaptureResult = (g_lastReadMapping == MAPPING_PREVIOUS) ? -5 : -6;
  MergeReadbackToPsxVuw(VRAMRead.x, VRAMRead.y,
                        VRAMRead.Width, VRAMRead.Height);
#endif
#ifdef DISP_DEBUG
#ifdef GLES_VRAM_COMMAND_FIXES
  if (ReadbackEnabled())
   {
#endif
    sprintf(txtbuffer,
          "VRB RESULT kind=%d capture=%d merged=%u src=%u/%u/%u "
          "changed=%u old=%08X new=%08X "
          "maskOnly=%u rgbChanged=%u mask=%u/%u "
          "full=%d partial=%d "
          "map=%u mv=%d cv=%d dirty=%d contam=%d mixed=%d untracked=%d "
          "live=%d/%u/%d/%d prev=%d/%u/%d/%d\r\n",
          g_lastReadMapping, g_lastCaptureResult, g_lastMergedPixels,
          g_lastMergedCurrentPixels, g_lastMergedPresentedPixels,
          g_lastMergedRebuildPixels,
          g_lastMergedChangedPixels,
          g_lastMergeOldHash, g_lastMergeNewHash,
          g_lastMergedMaskOnlyPixels,
          g_lastMergedRgbChangedPixels,
          g_lastMergeOldMaskPixels, g_lastMergeNewMaskPixels,
          CountEfbTiles(EFB_TILE_FULL),
          CountEfbTiles(EFB_TILE_PARTIAL),
          g_activeMap.map_id, g_activeMap.map_valid,
          g_activeMap.content_valid, g_activeMap.content_dirty,
          g_efbContaminated, g_mixedMappingSeen, g_untrackedEfbWrite,
          LIVE_SNAP()->valid, LIVE_SNAP()->map_id, LIVE_SNAP()->source,
          CountSnapshotTiles(LIVE_SNAP(), EFB_TILE_FULL),
          PREV_SNAP()->valid, PREV_SNAP()->map_id, PREV_SNAP()->source,
          CountSnapshotTiles(PREV_SNAP(), EFB_TILE_FULL));
    writeLogFile(txtbuffer);
#ifdef GLES_VRAM_COMMAND_FIXES
   }
#endif
#endif
  g_readbackState = READBACK_DONE;
 }

#ifdef GLES_VRAM_COMMAND_FIXES
for(i=0;i<iSize;i++)
 {
  uint32_t packed;
  int pixelOffset;
  int pixelsRead;

  if(VRAMRead.ColsRemaining<=0 || VRAMRead.RowsRemaining<=0)
   {FinishedVRAMRead();goto ENDREAD_GL;}

  pixelOffset=(VRAMRead.Height-VRAMRead.ColsRemaining)*VRAMRead.Width+
              (VRAMRead.Width-VRAMRead.RowsRemaining);
  pixelsRead=GlesVramCommandReadTransferWord(
   psxVuw,VRAMRead.x,VRAMRead.y,VRAMRead.Width,VRAMRead.Height,
   pixelOffset,&packed);
  if(pixelsRead<=0)
   {FinishedVRAMRead();goto ENDREAD_GL;}
  GPUdataRet=(unsigned long)packed;
  PUTLE32(pMem,GPUdataRet);pMem++;

  GlesVramCommandAdvanceTransfer(&VRAMRead);
  if(pixelsRead==2)
   GlesVramCommandAdvanceTransfer(&VRAMRead);
  if(VRAMRead.ColsRemaining<=0)
   {FinishedVRAMRead();goto ENDREAD_GL;}
 }
goto ENDREAD_GL;
#else
// adjust read ptr, if necessary
while(VRAMRead.ImagePtr>=psxVuw_eom)
 VRAMRead.ImagePtr-=iGPUHeight*1024;
while(VRAMRead.ImagePtr<psxVuw)
 VRAMRead.ImagePtr+=iGPUHeight*1024;

//if((iSize>1) &&
//   !(VRAMRead.x      == VRAMWrite.x     &&
//     VRAMRead.y      == VRAMWrite.y     &&
//     VRAMRead.Width  == VRAMWrite.Width &&
//     VRAMRead.Height == VRAMWrite.Height))
// if (iSize > 1)
// CheckVRamRead(VRAMRead.x,VRAMRead.y,
//               VRAMRead.x+VRAMRead.RowsRemaining,
//               VRAMRead.y+VRAMRead.ColsRemaining);

for(i=0;i<iSize;i++)
 {
  // do 2 seperate 16bit reads for compatibility (wrap issues)
  if ((VRAMRead.ColsRemaining > 0) && (VRAMRead.RowsRemaining > 0))
   {
    // lower 16 bit
    GPUdataRet=(unsigned long)GETLE16(VRAMRead.ImagePtr);

    VRAMRead.ImagePtr++;
    if(VRAMRead.ImagePtr>=psxVuw_eom) VRAMRead.ImagePtr-=iGPUHeight*1024;
    VRAMRead.RowsRemaining --;

    if(VRAMRead.RowsRemaining<=0)
     {
      VRAMRead.RowsRemaining = VRAMRead.Width;
      VRAMRead.ColsRemaining--;
      VRAMRead.ImagePtr += 1024 - VRAMRead.Width;
      if(VRAMRead.ImagePtr>=psxVuw_eom) VRAMRead.ImagePtr-=iGPUHeight*1024;
     }

    // higher 16 bit (always, even if it's an odd width)
    GPUdataRet|=(unsigned long)GETLE16(VRAMRead.ImagePtr)<<16;
    PUTLE32(pMem, GPUdataRet); pMem++;

    if(VRAMRead.ColsRemaining <= 0)
     {FinishedVRAMRead();goto ENDREAD_GL;}

    VRAMRead.ImagePtr++;
    if(VRAMRead.ImagePtr>=psxVuw_eom) VRAMRead.ImagePtr-=iGPUHeight*1024;
    VRAMRead.RowsRemaining--;
    if(VRAMRead.RowsRemaining<=0)
     {
      VRAMRead.RowsRemaining = VRAMRead.Width;
      VRAMRead.ColsRemaining--;
      VRAMRead.ImagePtr += 1024 - VRAMRead.Width;
      if(VRAMRead.ImagePtr>=psxVuw_eom) VRAMRead.ImagePtr-=iGPUHeight*1024;
     }
    if(VRAMRead.ColsRemaining <= 0)
     {FinishedVRAMRead();goto ENDREAD_GL;}
   }
  else {FinishedVRAMRead();goto ENDREAD_GL;}
 }
#endif

ENDREAD_GL:
GPUIsIdle;
 #ifdef DISP_DEBUG
 //sprintf(txtbuffer, "GL_GPUreadDataMem %08x \r\n", GPUdataRet);
 //writeLogFile(txtbuffer);
 #endif // DISP_DEBUG
}

unsigned long CALLBACK GL_GPUreadData(void)
{
 unsigned long l;
 GL_GPUreadDataMem(&l,1);
 return GPUdataRet;
}

////////////////////////////////////////////////////////////////////////
// helper table to know how much data is used by drawing commands
////////////////////////////////////////////////////////////////////////
extern const unsigned char primTableCX[];
//const unsigned char primTableCX[256] =
//{
//    // 00
//    0,0,3,0,0,0,0,0,
//    // 08
//    0,0,0,0,0,0,0,0,
//    // 10
//    0,0,0,0,0,0,0,0,
//    // 18
//    0,0,0,0,0,0,0,0,
//    // 20
//    4,4,4,4,7,7,7,7,
//    // 28
//    5,5,5,5,9,9,9,9,
//    // 30
//    6,6,6,6,9,9,9,9,
//    // 38
//    8,8,8,8,12,12,12,12,
//    // 40
//    3,3,3,3,0,0,0,0,
//    // 48
////    5,5,5,5,6,6,6,6,      //FLINE
//    254,254,254,254,254,254,254,254,
//    // 50
//    4,4,4,4,0,0,0,0,
//    // 58
////    7,7,7,7,9,9,9,9,    //    LINEG3    LINEG4
//    255,255,255,255,255,255,255,255,
//    // 60
//    3,3,3,3,4,4,4,4,    //    TILE    SPRT
//    // 68
//    2,2,2,2,3,3,3,3,    //    TILE1
//    // 70
//    2,2,2,2,3,3,3,3,
//    // 78
//    2,2,2,2,3,3,3,3,
//    // 80
//    4,0,0,0,0,0,0,0,
//    // 88
//    0,0,0,0,0,0,0,0,
//    // 90
//    0,0,0,0,0,0,0,0,
//    // 98
//    0,0,0,0,0,0,0,0,
//    // a0
//    3,0,0,0,0,0,0,0,
//    // a8
//    0,0,0,0,0,0,0,0,
//    // b0
//    0,0,0,0,0,0,0,0,
//    // b8
//    0,0,0,0,0,0,0,0,
//    // c0
//    3,0,0,0,0,0,0,0,
//    // c8
//    0,0,0,0,0,0,0,0,
//    // d0
//    0,0,0,0,0,0,0,0,
//    // d8
//    0,0,0,0,0,0,0,0,
//    // e0
//    0,1,1,1,1,1,1,0,
//    // e8
//    0,0,0,0,0,0,0,0,
//    // f0
//    0,0,0,0,0,0,0,0,
//    // f8
//    0,0,0,0,0,0,0,0
//};

////////////////////////////////////////////////////////////////////////
// processes data send to GPU data register
////////////////////////////////////////////////////////////////////////

void CALLBACK GL_GPUwriteDataMem(unsigned long * pMem, int iSize)
{
unsigned char command;
unsigned long gdata=0;
int i=0;
GPUIsBusy;
GPUIsNotReadyForCommands;

STARTVRAM_GL:

if(iDataWriteMode==DR_VRAMTRANSFER)
 {
//    #if defined(DISP_DEBUG)
//    sprintf ( txtbuffer, "GPUwriteDataMem DR_VRAMTRANSFER %d \r\n", iSize );
//    writeLogFile(txtbuffer);
//    #endif // DISP_DEBUG

#ifdef GLES_VRAM_COMMAND_FIXES
  while(VRAMWrite.ColsRemaining>0)
   {
    while(VRAMWrite.RowsRemaining>0)
     {
      if(i>=iSize) {goto ENDVRAM_GL;}
      i++;
      gdata=GETLE32(pMem);pMem++;

      GlesVramCommandWriteTransferPixel((unsigned short)gdata);
      if(VRAMWrite.ColsRemaining<=0)
       {
        FinishedVRAMWrite();
        goto ENDVRAM_GL;
       }

      GlesVramCommandWriteTransferPixel((unsigned short)(gdata>>16));
      if(VRAMWrite.ColsRemaining<=0)
       {
        FinishedVRAMWrite();
        goto ENDVRAM_GL;
       }
     }
   }
  FinishedVRAMWrite();
#else
  // make sure we are in vram
  while(VRAMWrite.ImagePtr>=psxVuw_eom)
   VRAMWrite.ImagePtr-=iGPUHeight*1024;
  while(VRAMWrite.ImagePtr<psxVuw)
   VRAMWrite.ImagePtr+=iGPUHeight*1024;

  // now do the loop
  while(VRAMWrite.ColsRemaining>0)
   {
    while(VRAMWrite.RowsRemaining>0)
     {
      if(i>=iSize) {goto ENDVRAM_GL;}
      i++;

       gdata=GETLE32(pMem); pMem++;

       // Write odd pixel - Wrap from beginning to next index if going past GPU width
       if (VRAMWrite.Width + VRAMWrite.x - VRAMWrite.RowsRemaining >= 1024)
       {
           PUTLE16(((VRAMWrite.ImagePtr++) - 1024), (unsigned short)gdata);
       }
       else
       {
           PUTLE16(VRAMWrite.ImagePtr++, (unsigned short)gdata);
       }
      if(VRAMWrite.ImagePtr>=psxVuw_eom) VRAMWrite.ImagePtr-=iGPUHeight*1024;
      VRAMWrite.RowsRemaining --;

      if(VRAMWrite.RowsRemaining <= 0)
       {
        VRAMWrite.ColsRemaining--;
        if (VRAMWrite.ColsRemaining <= 0)             // last pixel is odd width
         {
           gdata=(gdata&0xFFFF)|(((unsigned long)GETLE16(VRAMWrite.ImagePtr))<<16);
          FinishedVRAMWrite();
          goto ENDVRAM_GL;
         }
        VRAMWrite.RowsRemaining = VRAMWrite.Width;
        VRAMWrite.ImagePtr += 1024 - VRAMWrite.Width;
       }

       // Write even pixel - Wrap from beginning to next index if going past GPU width
       if (VRAMWrite.Width + VRAMWrite.x - VRAMWrite.RowsRemaining >= 1024)
       {
           PUTLE16(((VRAMWrite.ImagePtr++) - 1024), (unsigned short)(gdata>>16));
       }
       else
       {
           PUTLE16(VRAMWrite.ImagePtr++, (unsigned short)(gdata>>16));
       }
      if(VRAMWrite.ImagePtr>=psxVuw_eom) VRAMWrite.ImagePtr-=iGPUHeight*1024;
      VRAMWrite.RowsRemaining --;
     }

    VRAMWrite.RowsRemaining = VRAMWrite.Width;
    VRAMWrite.ColsRemaining--;
    VRAMWrite.ImagePtr += 1024 - VRAMWrite.Width;
   }

  FinishedVRAMWrite();
#endif
 }

ENDVRAM_GL:

if(iDataWriteMode==DR_NORMAL)
 {
//    #if defined(DISP_DEBUG)
//    sprintf ( txtbuffer, "GPUwriteDataMem DR_NORMAL %d \r\n", iSize );
//    writeLogFile(txtbuffer);
//    #endif // DISP_DEBUG

  void (* *primFunc)(unsigned char *);
  if(bSkipNextFrame) primFunc=primTableSkipGx;
  else               primFunc=primTableJGx;

  for(;i<iSize;)
   {
    if(iDataWriteMode==DR_VRAMTRANSFER) goto STARTVRAM_GL;

     gdata=GETLE32(pMem); pMem++; i++;

    if(gpuDataC == 0)
     {
      command = (unsigned char)((gdata>>24) & 0xff);

      if(primTableCX[command])
       {
        gpuDataC = primTableCX[command];
        gpuCommand = command;
         PUTLE32(&gpuDataM[0], gdata);
        gpuDataP = 1;
#ifdef GLES_VRAM_COMMAND_FIXES
        gpuDataWords=1;
#endif
       }
      else continue;
     }
    else
     {
       PUTLE32(&gpuDataM[gpuDataP], gdata);
#ifdef GLES_VRAM_COMMAND_FIXES
       gpuDataWords=gpuDataP+1;
#endif
      if(gpuDataC>128)
       {
        if((gpuDataC==254 && gpuDataP>=3) ||
           (gpuDataC==255 && gpuDataP>=4 && !(gpuDataP&1)))
         {
           if((gdata & 0xF000F000) == 0x50005000)
           gpuDataP=gpuDataC-1;
         }
       }
      gpuDataP++;
     }

    if(gpuDataP == gpuDataC)
     {
#ifdef GLES_VRAM_COMMAND_FIXES
      long packetWords=gpuDataWords;
#if defined(DISP_DEBUG) && defined(GLES_VRAM_FLOW_DIAG)
      GlesGpuDrawFootprint flowFootprint;
      int flowFootprintValid=0;
      unsigned int flowSubmittedBefore;
#endif
#endif
      gpuDataC=gpuDataP=0;
#ifdef GLES_VRAM_COMMAND_FIXES
      gpuDataWords=0;
      if(GlesGpuSelectedHandlerMayDraw(primFunc[gpuCommand]))
       {
#if defined(DISP_DEBUG) && defined(GLES_VRAM_FLOW_DIAG)
        flowFootprintValid=GlesGpuObserveDrawFootprint(
         gpuDataM,(int)packetWords,&flowFootprint);
#else
       GlesGpuObserveDrawFootprint(gpuDataM,(int)packetWords,NULL);
#endif
       }
#endif
#if defined(DISP_DEBUG) && defined(GLES_VRAM_FLOW_DIAG) && defined(GLES_VRAM_COMMAND_FIXES)
      flowSubmittedBefore=g_debugDrawSubmitted;
#endif
      BeginEfbDrawContext();
      primFunc[gpuCommand]((unsigned char *)gpuDataM);
#if defined(DISP_DEBUG) && defined(GLES_VRAM_FLOW_DIAG) && defined(GLES_VRAM_COMMAND_FIXES)
      if(flowFootprintValid)
       GlesGpuLogFlowCommandResult(
        gpuCommand,(int)packetWords,&flowFootprint,flowSubmittedBefore);
#endif
      EndEfbDrawContext();

       if (dwActFixes & AUTO_FIX_GPU_BUSY)      // hack for emulating "gpu busy" in some games
       iFakePrimBusy=4;
     }
   }
 }

GPUdataRet=gdata;

GPUIsReadyForCommands;
GPUIsIdle;
}

////////////////////////////////////////////////////////////////////////

void CALLBACK GL_GPUwriteData(unsigned long gdata)
{
 PUTLE32(&gdata, gdata);
 GL_GPUwriteDataMem(&gdata,1);
}


////////////////////////////////////////////////////////////////////////
// Pete Special: make an 'intelligent' dma chain check (<-Tekken3)
////////////////////////////////////////////////////////////////////////

static unsigned long lUsedAddr[3];

__inline BOOL CheckForEndlessLoop(unsigned long laddr)
{
if(laddr==lUsedAddr[1]) return TRUE;
if(laddr==lUsedAddr[2]) return TRUE;

if(laddr<lUsedAddr[0]) lUsedAddr[1]=laddr;
else                   lUsedAddr[2]=laddr;
lUsedAddr[0]=laddr;
return FALSE;
}

////////////////////////////////////////////////////////////////////////
// core gives a dma chain to gpu: same as the gpuwrite interface funcs
////////////////////////////////////////////////////////////////////////

long CALLBACK GL_GPUdmaChain(unsigned long * baseAddrL, unsigned long addr, uint32_t *progress_addr, int32_t *cycles_last_cmd)
{
 unsigned char * baseAddrB;
 unsigned int DMACommandCounter = 0;
 long dmaWords = 0;


if(bIsFirstFrame) GLinitialize(NULL, NULL);

GPUIsBusy;

lUsedAddr[0]=lUsedAddr[1]=lUsedAddr[2]=0xffffff;

baseAddrB = (unsigned char*) baseAddrL;

do
 {
  if(iGPUHeight==512) addr&=0x1FFFFC;

  if(DMACommandCounter++ > 2000000) break;
  if(CheckForEndlessLoop(addr)) break;

   short count = baseAddrB[addr+3];
   dmaWords += 1 + count;

   unsigned long dmaMem=addr+4;

  if(count>0) GL_GPUwriteDataMem(&baseAddrL[dmaMem>>2],count);

   addr = GETLE32(&baseAddrL[addr>>2])&0xffffff;
  }
 while (!(addr & 0x800000)); // contrary to some documentation, the end-of-linked-list marker is not actually 0xFF'FFFF
                             // any pointer with bit 23 set will do.

 GPUIsIdle;

 return dmaWords;
}

////////////////////////////////////////////////////////////////////////
// save state funcs
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////

long CALLBACK GL_GPUfreeze(unsigned long ulGetFreezeData,GPUFreeze_t * pF)
{
if(ulGetFreezeData==2)
 {
  long lSlotNum=*((long *)pF);
  if(lSlotNum<0) return 0;
  if(lSlotNum>8) return 0;
  //lSelectedSlot=lSlotNum+1;
  return 1;
 }

if(!pF)                    return 0;
if(pF->ulFreezeVersion!=1) return 0;

if(ulGetFreezeData==1)
 {
  pF->ulStatus=STATUSREG;
  memcpy(pF->ulControl,ulStatusControl,256*sizeof(unsigned long));
  //memcpy(pF->psxVRam,  psxVub,         1024*iGPUHeight*2);

  return 1;
 }

if(ulGetFreezeData!=0) return 0;

STATUSREG=pF->ulStatus;
memcpy(ulStatusControl,pF->ulControl,256*sizeof(unsigned long));
//memcpy(psxVub,         pF->psxVRam,  1024*iGPUHeight*2);

ResetTextureArea(TRUE);

 GL_GPUwriteStatus(ulStatusControl[0]);
 GL_GPUwriteStatus(ulStatusControl[1]);
 GL_GPUwriteStatus(ulStatusControl[2]);
 GL_GPUwriteStatus(ulStatusControl[3]);
 GL_GPUwriteStatus(ulStatusControl[8]);
 GL_GPUwriteStatus(ulStatusControl[6]);
 GL_GPUwriteStatus(ulStatusControl[7]);
 GL_GPUwriteStatus(ulStatusControl[5]);
 GL_GPUwriteStatus(ulStatusControl[4]);
 return 1;
}

////////////////////////////////////////////////////////////////////////
// special "emu infos" / "emu effects" functions
////////////////////////////////////////////////////////////////////////

// pcsx-rearmed callbacks
void CALLBACK GL_GPUrearmedCallbacks(const struct rearmed_cbs *_cbs)
{
   #ifdef DISP_DEBUG
 //writeLogFile("GL_GPUrearmedCallbacks 0\r\n");
 #endif // DISP_DEBUG
//   gpu.frameskip.set = _cbs->frameskip;
//  gpu.frameskip.advice = &_cbs->fskip_advice;
//  gpu.frameskip.force = &_cbs->fskip_force;
//  gpu.frameskip.dirty = (void *)&_cbs->fskip_dirty;
//  gpu.frameskip.active = 0;
//  gpu.frameskip.frame_ready = 1;
//  gpu.state.hcnt = _cbs->gpu_hcnt;
//  gpu.state.frame_count = _cbs->gpu_frame_count;
//  gpu.state.allow_interlace = _cbs->gpu_neon.allow_interlace;
//  gpu.state.enhancement_enable = _cbs->gpu_neon.enhancement_enable;
//  if (gpu.state.screen_centering_type != _cbs->screen_centering_type
//      || gpu.state.screen_centering_x != _cbs->screen_centering_x
//      || gpu.state.screen_centering_y != _cbs->screen_centering_y) {
//    gpu.state.screen_centering_type = _cbs->screen_centering_type;
//    gpu.state.screen_centering_x = _cbs->screen_centering_x;
//    gpu.state.screen_centering_y = _cbs->screen_centering_y;
//    update_width();
//    update_height();
//  }
//
//  gpu.mmap = _cbs->mmap;
//  gpu.munmap = _cbs->munmap;
//  gpu.gpu_state_change = _cbs->gpu_state_change;
//
//  // delayed vram mmap
//  if (gpu.vram == NULL)
//    map_vram();
//
//  if (_cbs->pl_vout_set_raw_vram)
//    _cbs->pl_vout_set_raw_vram(gpu.vram);
  #ifdef DISP_DEBUG
 //writeLogFile("GL_GPUrearmedCallbacks 1\r\n");
 #endif // DISP_DEBUG
  renderer_set_config(_cbs);
  #ifdef DISP_DEBUG
 //writeLogFile("GL_GPUrearmedCallbacks 2\r\n");
 #endif // DISP_DEBUG
  vout_set_config(_cbs);
}

static void flipEGL(void)
{
    int presentSubmitted;

    /* Parasite Eve II alternates two PS1 VRAM display pages while the GX
     * renderer has only one EFB. Keeping that EFB after a present allows a
     * translucent full-screen warning effect from one page to become the
     * blend destination of the other page. Restore the original clear-after-
     * copy behavior only for this title. */
    if (dwActFixes & AUTO_FIX_PE2_CLEAR_EFB)
        canClearFrameBuf = TRUE;

    #ifdef DISP_DEBUG
#if defined(GLES_VRAM_FLOW_DIAG) && defined(GLES_VRAM_COMMAND_FIXES)
    sprintf(txtbuffer,
            "FMD FRAME=%u cmd=F%u/A%u/M%u/G%u "
            "fill=%d,%d,%d,%d:%d/%d/%d:%d:%d>%d:%06X "
            "a0=%d,%d,%d,%d move=%d,%d>%d,%d,%d,%d:u%d "
            "disp=%d,%d,%d,%d prev=%d,%d,%d,%d "
            "active=%d/%u:%d,%d-%d,%d "
            "area=%d,%d-%d,%d off=%d,%d screen=%d,%d-%d,%d "
            "draw=%u/%u/%u/%u/%u/%u/%u/%u cover=%u/%u/%u "
            "read=%u/%u/%u/%u:%d/%d "
            "pending=%d/%d:%u/%u flags=%d/%d/%d:%d,%d-%d,%d:%d "
            "upload=%u/%u/%u:p%d:m%u:b%x:%d,%d-%d,%d:n%u:h%08X:t%u "
            "stp=%u:%d/%d base=%u:%02X/%06X:"
            "%d,%d-%d,%d>%d,%d-%d,%d>%d,%d-%d,%d:"
            "%d/%d/%d/%d:z%d:s%d\r\n",
            g_textureDiagFrame,
            g_vramFlowDiag.fills, g_vramFlowDiag.loads,
            g_vramFlowDiag.moves, g_vramFlowDiag.gp1Maps,
            g_vramFlowDiag.fillX, g_vramFlowDiag.fillY,
            g_vramFlowDiag.fillW, g_vramFlowDiag.fillH,
            g_vramFlowDiag.fillCurrent, g_vramFlowDiag.fillNext,
            g_vramFlowDiag.fillSubmitted,
            g_vramFlowDiag.fillPendingManaged,
            g_vramFlowDiag.fillPendingBefore,
            g_vramFlowDiag.fillPendingAfter,
            g_vramFlowDiag.fillColor & 0x00ffffffU,
            g_vramFlowDiag.loadX, g_vramFlowDiag.loadY,
            g_vramFlowDiag.loadW, g_vramFlowDiag.loadH,
            g_vramFlowDiag.moveX0, g_vramFlowDiag.moveY0,
            g_vramFlowDiag.moveX1, g_vramFlowDiag.moveY1,
            g_vramFlowDiag.moveW, g_vramFlowDiag.moveH,
            g_vramFlowDiag.moveUploaded,
            PSXDisplay.DisplayPosition.x, PSXDisplay.DisplayPosition.y,
            PSXDisplay.DisplayEnd.x, PSXDisplay.DisplayEnd.y,
            PreviousPSXDisplay.DisplayPosition.x,
            PreviousPSXDisplay.DisplayPosition.y,
            PreviousPSXDisplay.DisplayEnd.x,
            PreviousPSXDisplay.DisplayEnd.y,
            g_activeMap.map_valid, g_activeMap.map_id,
            g_activeMap.vram_x0, g_activeMap.vram_y0,
            g_activeMap.vram_x1, g_activeMap.vram_y1,
            PSXDisplay.DrawArea.x0, PSXDisplay.DrawArea.y0,
            PSXDisplay.DrawArea.x1, PSXDisplay.DrawArea.y1,
            PSXDisplay.DrawOffset.x, PSXDisplay.DrawOffset.y,
            screenX, screenY, screenX1, screenY1,
            g_drawFootprintStats.commands-g_vramFlowPrevDrawCommands,
            g_drawFootprintStats.withPixels-g_vramFlowPrevDrawPixels,
            g_debugDrawSubmitted-g_vramFlowPrevEfbSubmits,
            g_drawFootprintStats.activeMapHits-g_vramFlowPrevDrawActive,
            g_drawFootprintStats.previousDisplayHits-g_vramFlowPrevDrawPrevious,
            g_drawFootprintStats.currentDisplayHits-g_vramFlowPrevDrawCurrent,
            g_drawFootprintStats.outside-g_vramFlowPrevDrawOutside,
            g_drawFootprintStats.pendingUploadHits-g_vramFlowPrevDrawPending,
            g_vramFlowDiag.coversActive,
            g_vramFlowDiag.coversPrevious,
            g_vramFlowDiag.coversCurrent,
            g_readBarrierCalls-g_vramFlowPrevReadCalls,
            g_readBarrierFastPaths-g_vramFlowPrevReadFast,
            g_readBarrierCaptures-g_vramFlowPrevReadCaptures,
            g_readBarrierUnresolved-g_vramFlowPrevReadUnresolved,
            g_lastReadMapping, g_lastCaptureResult,
            GlesGpuPendingUploadsManaged(), g_pendingCpuUploads.count,
            g_pendingCpuUploadFlushes-g_vramFlowPrevPendingFlushes,
            g_pendingCpuUploadFallbacks-g_vramFlowPrevPendingFallbacks,
            lClearOnSwap, canClearFrameBuf, bNeedUploadAfter,
            xrUploadArea.x0, xrUploadArea.y0,
            xrUploadArea.x1, xrUploadArea.y1, needFlipEGL,
            g_vramFlowDiag.uploadScreenCalls,
            g_vramFlowDiag.uploadFullCalls,
            g_vramFlowDiag.uploadChunks,
            g_vramFlowDiag.uploadPosition,
            g_vramFlowDiag.uploadMapId,
            g_vramFlowDiag.uploadDrawnBefore,
            g_vramFlowDiag.uploadX0, g_vramFlowDiag.uploadY0,
            g_vramFlowDiag.uploadX1, g_vramFlowDiag.uploadY1,
            g_vramFlowDiag.uploadNonZeroPixels,
            g_vramFlowDiag.uploadSourceHash,
            g_vramFlowDiag.uploadTextureType,
            g_vramFlowDiag.stpWrites,
            g_vramFlowDiag.stpSet, g_vramFlowDiag.stpCheck,
            g_vramFlowDiag.baseCovers,
            g_vramFlowDiag.baseOpcode, g_vramFlowDiag.baseColor,
            g_vramFlowDiag.baseRawX0, g_vramFlowDiag.baseRawY0,
            g_vramFlowDiag.baseRawX1, g_vramFlowDiag.baseRawY1,
            g_vramFlowDiag.baseVramX0, g_vramFlowDiag.baseVramY0,
            g_vramFlowDiag.baseVramX1, g_vramFlowDiag.baseVramY1,
            g_vramFlowDiag.baseEfbX0, g_vramFlowDiag.baseEfbY0,
            g_vramFlowDiag.baseEfbX1, g_vramFlowDiag.baseEfbY1,
            g_vramFlowDiag.baseSemi, g_vramFlowDiag.baseAbr,
            g_vramFlowDiag.baseMaskSet,
            g_vramFlowDiag.baseMaskCheck,
            g_vramFlowDiag.baseZMillionths,
            g_vramFlowDiag.baseSubmitted);
    TextureDiagAppend(txtbuffer);
    g_vramFlowPrevDrawCommands=g_drawFootprintStats.commands;
    g_vramFlowPrevDrawPixels=g_drawFootprintStats.withPixels;
    g_vramFlowPrevDrawActive=g_drawFootprintStats.activeMapHits;
    g_vramFlowPrevDrawPrevious=g_drawFootprintStats.previousDisplayHits;
    g_vramFlowPrevDrawCurrent=g_drawFootprintStats.currentDisplayHits;
    g_vramFlowPrevDrawOutside=g_drawFootprintStats.outside;
    g_vramFlowPrevDrawPending=g_drawFootprintStats.pendingUploadHits;
    g_vramFlowPrevEfbSubmits=g_debugDrawSubmitted;
    g_vramFlowPrevReadCalls=g_readBarrierCalls;
    g_vramFlowPrevReadFast=g_readBarrierFastPaths;
    g_vramFlowPrevReadCaptures=g_readBarrierCaptures;
    g_vramFlowPrevReadUnresolved=g_readBarrierUnresolved;
    g_vramFlowPrevPendingFlushes=g_pendingCpuUploadFlushes;
    g_vramFlowPrevPendingFallbacks=g_pendingCpuUploadFallbacks;
    memset(&g_vramFlowDiag,0,sizeof(g_vramFlowDiag));
#endif
    sprintf(txtbuffer,
            "TDI PRESENT frame=%u events=%u draws=%u efbclears=%u "
            "swapclear=%d shown=%d drawn=%d disp=%d,%d prev=%d,%d "
            "%dx%d rgb=%d\r\n",
            g_textureDiagFrame, g_textureDiagEvent, g_textureDiagDraw,
            g_textureDiagEfbClears, canClearFrameBuf, canShowFps,
            iDrawnSomething,
            PSXDisplay.DisplayPosition.x, PSXDisplay.DisplayPosition.y,
            PreviousPSXDisplay.DisplayPosition.x,
            PreviousPSXDisplay.DisplayPosition.y,
            PSXDisplay.DisplayMode.x, PSXDisplay.DisplayMode.y,
            PSXDisplay.RGB24);
    DEBUG_print(txtbuffer, DBG_SPU3);
    TextureDiagAppend(txtbuffer);
    #endif // DISP_DEBUG

    CapturePresentedEfbSnapshot();

    if (canShowFps)
    {
        // Write menu/debug text on screen
        showFpsAndDebugInfo();
        g_efbContaminated = TRUE;
    }

    // Check if TVMode needs to be changed (240 or 480 lines)
    if (originalMode == ORIGINALMODE_ENABLE)
    {
        extern int backFromMenu;
        if(backFromMenu)
        {
            backFromMenu = 0;
            gx_vout_wait_idle();
            switchToTVMode(PSXDisplay.DisplayModeNew.x, PSXDisplay.DisplayModeNew.y, 0);
        }
    }

#ifdef DISP_DEBUG
#if defined(GLES_VRAM_FLOW_DIAG) && defined(GLES_VRAM_COMMAND_FIXES)
    gx_vout_set_diag_frame(g_textureDiagFrame);
#endif
#endif
    presentSubmitted = gx_vout_render(canClearFrameBuf);

#ifdef DISP_DEBUG
#if defined(GLES_VRAM_FLOW_DIAG) && defined(GLES_VRAM_COMMAND_FIXES)
    {
        unsigned int copySubmitted;
        unsigned int copySkipped;
        unsigned int copyCompleted;
        unsigned int copyPublished;
        int copyInflight;
        int copyReady;
        unsigned int copiedFrame;
        unsigned int copiedHash[4];
        unsigned int copiedSamples;

        gx_vout_get_diag(&copySubmitted, &copySkipped,
                         &copyCompleted, &copyPublished,
                         &copyInflight, &copyReady);
        gx_vout_get_present_hash(&copiedFrame, copiedHash,
                                 &copiedSamples);
        sprintf(txtbuffer,
                "TDI COPY frame=%u result=%d totals=%u/%u/%u/%u "
                "inflight=%d ready=%d xfb=%u:%u:"
                "%08X/%08X/%08X/%08X\r\n",
                g_textureDiagFrame, presentSubmitted,
                copySubmitted, copySkipped, copyCompleted, copyPublished,
                copyInflight, copyReady, copiedFrame, copiedSamples,
                copiedHash[0], copiedHash[1],
                copiedHash[2], copiedHash[3]);
        TextureDiagAppend(txtbuffer);
    }
#endif
    TextureDiagFlush();
#endif

    if (presentSubmitted && canClearFrameBuf)
        EfbDiscardedAfterPresent();

    clearLargeRange = 0;
    uploadedScreen = FALSE;
    needFlipEGL = presentSubmitted ? FALSE : TRUE;
    if (presentSubmitted)
        canClearFrameBuf = FALSE;
    canShowFps = FALSE;
    RGB24Uploaded = 0;
    glSetLoadMtxFlg();

    extern void resetTexCacheInfo(void);
    resetTexCacheInfo();
    ResetFramebufferTextureCapture();

#ifdef DISP_DEBUG
    g_textureDiagFrame++;
    g_textureDiagDraw = 0;
    g_textureDiagEvent = 0;
    g_textureDiagEfbClears = 0;
#endif
}

#include "../Gamecube/wiiSXconfig.h"
extern char screenMode;

long GL_GPUopen()
{
 int ret;

 InitFPS();

 GPUsetframelimit(0);

 iResX = 640;
 iResY = 480;
 rRatioRect.left   = 0;
// if (screenMode != SCREENMODE_4x3)
// {
//     rRatioRect.left   = -104;
//     iResX = 744;
// }
 iOffscreenDrawing = 0;
 rRatioRect.top=0;
 rRatioRect.right  = iResX;
 rRatioRect.bottom = iResY;

 bIsFirstFrame = TRUE;
 bDisplayNotSet = TRUE;
 bSetClip = TRUE;
 CSTEXTURE = CSVERTEX = CSCOLOR = 0;
 canClearFrameBuf = FALSE;

 InitializeTextureStore();                             // init texture mem

 ret = GLinitialize(NULL, NULL);

 gx_vout_open();

 ogx_draw_submitted_cb = OnEfbDrawSubmitted;

 return ret;
}

long GL_GPUclose(void)
{
 ogx_draw_submitted_cb = NULL;
 GlesGpuNoSwapPageShutdown();
 ResetVramReadbackState();
 GLcleanup();                                          // close OGL
 return 0;
}

gpu_t glesGpu = {
    GL_GPUopen,
    GL_GPUinit,
    GL_GPUshutdown,
    GL_GPUclose,
    GL_GPUwriteStatus,
    GL_GPUwriteData,
    GL_GPUreadStatus,
    GL_GPUreadData,
    GL_GPUdmaChain,
    GL_GPUupdateLace,
    GL_GPUfreeze,
    GL_GPUreadDataMem,
    GL_GPUwriteDataMem,
    GPUsetframelimit
};
