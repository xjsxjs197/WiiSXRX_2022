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
#include "gpuVramRect.h"
#include "gpuTextureReadBarrier.h"
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

static void ResetVramReadbackState(void);
static void BuildActiveMapFromDisplay(void);
static inline unsigned short ReadGXRGB5A3PixelRaw(
    const unsigned char *buf, int texWidth, int px, int py);
static inline unsigned short GXRGB5A3ToPSX15(unsigned short gx);
extern GXRModeObj *vmode;     /*** Graphics Mode Object ***/

#ifdef DISP_DEBUG
/* Dino Crisis 2 pause-feedback diagnosis.  gpuTexture.c is included before
 * gpuVramReadback.inc, so keep the small cross-stage trace state here. */
static unsigned int g_dc2PauseDiagBarrierSerial;
static unsigned int g_dc2PauseDiagEventSerial;
static unsigned int g_dc2PauseDiagEventLogs;
static unsigned int g_dc2PauseDiagTextureLogs;
static unsigned int g_dc2PauseDiagDrawLogs;
static unsigned int g_dc2PauseDiagInvalidateLogs;
static unsigned int g_dc2PauseDiagDrawBudget;
static unsigned int g_dc2PauseFeedbackSkips;
static int g_dc2PauseDiagLastResult;
static unsigned int g_dc2PauseDiagLastChanged;
#endif

/* gpuTexture.c is included before gpuVramReadback.inc, so the barrier entry
 * and its readback enablement need forward declarations in this TU. */
static BOOL ReadbackEnabled(void);
static VramFreshResult EnsureVramReadFresh(
    const VramReadDependency *dependency);
static VramFreshResult EnsureVramReadFreshEx(
    const VramReadDependency *dependency,
    unsigned int *changedTilesOut);
static BOOL ShouldSkipDC2PauseFeedbackMaterialize(
    const VramReadDependency *dependency, int pageid, int textureMode);

#include "gpuDraw.c"
#include "gpuTexture.c"
#include "gpuVramReadback.inc"
#include "gpuPrim.c"

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
bVramWriteTransferActive = FALSE;

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
   bVramWriteTransferActive=FALSE;
   PSXDisplay.DrawOffset.x=PSXDisplay.DrawOffset.y=0;
   drawX=drawY=0;drawW=drawH=0;
   sSetMask=0;lSetMask=0;bCheckMask=FALSE;iSetMask=0;
   usMirror=0;
   GlobalTextAddrX=0;GlobalTextAddrY=0;
   GlobalTextTP=0;GlobalTextABR=0;
   PSXDisplay.RGB24=FALSE;
   PSXDisplay.Interlaced=FALSE;
   bUsingTWin = FALSE;
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

#if T6_BARRIER_DIAG
   g_t6A0LastDmaOldWriteMode = iDataWriteMode;
#endif
   iDataWriteMode=iDataReadMode=DR_NORMAL;
   if(gdata==0x02) iDataWriteMode=DR_VRAMTRANSFER;
   if(gdata==0x03) iDataReadMode =DR_VRAMTRANSFER;
#if T6_BARRIER_DIAG
   g_t6A0DmaSerial++;
   g_t6A0LastDmaMode = (int)gdata;
   g_t6A0LastDmaNewWriteMode = iDataWriteMode;
#endif

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
BOOL bVramWriteTransferActive=FALSE;

#if T6_BARRIER_DIAG
enum {
 T6_CPU_NEWER_PROBE_MAX_RELEVANT = 64,
 T6_CPU_NEWER_PROBE_PASS_TARGET = 4
};
#endif

static inline void FinishedVRAMWrite(void)
{
#if T6_BARRIER_DIAG
 uint64_t provenanceSeq = 0;
 unsigned int baselineCapturesBefore = g_t6RebuildBaselineCaptures;
 int a0NeedUploadAtFinish = bNeedWriteUpload;
 int a0WriteModeAtFinish = iDataWriteMode;
 int a0RowsAtFinish = VRAMWrite.RowsRemaining;
 int a0ColsAtFinish = VRAMWrite.ColsRemaining;

 g_t6A0CheckOutcomeFlags = 0;
 g_t6A0FinishSerial++;
#endif
 if (ReadbackEnabled())
 {
  unsigned int provenanceFlags = bNeedWriteUpload ?
   T6_CPU_WRITE_FLAG_UPLOAD_PENDING : 0;

#if T6_BARRIER_DIAG
  provenanceSeq =
#endif
  MarkCpuVramWriteKind(VRAMWrite.x, VRAMWrite.y,
                       VRAMWrite.Width, VRAMWrite.Height,
                       T6_CPU_WRITE_A0, provenanceFlags);
#if T6_BARRIER_DIAG
  if (provenanceSeq != 0)
   T6CpuWriteProvenanceUpdateA0Flow(
       &g_t6CpuWriteProvenance, provenanceSeq,
       g_t6A0TransferGeneration, g_t6A0FinishSerial,
       g_t6A0DmaSerial, g_t6A0ArmDmaSerial,
       g_t6A0TransferArmed, a0NeedUploadAtFinish,
       a0WriteModeAtFinish, a0RowsAtFinish, a0ColsAtFinish,
       g_t6A0ArmX, g_t6A0ArmY, g_t6A0ArmW, g_t6A0ArmH,
       g_t6A0LastDmaMode, g_t6A0LastDmaOldWriteMode,
       g_t6A0LastDmaNewWriteMode);
  /* Real CPU-newer window: MarkCpuVramWriteKind() has committed the A0
   * epoch, while CheckWriteUpdate() below has not uploaded/rebuilt EFB yet.
   * Bound both relevant attempts and successful samples so SD logging and
   * the read-only shadow cannot become a new steady-state cost. */
  if (provenanceSeq != 0 &&
      isLogFileEnabled() &&
      g_t6CpuNewerProbeRelevant < T6_CPU_NEWER_PROBE_MAX_RELEVANT &&
      g_t6CpuNewerProbePasses < T6_CPU_NEWER_PROBE_PASS_TARGET &&
      ClassifyReadMapping(VRAMWrite.x, VRAMWrite.y,
                          VRAMWrite.Width, VRAMWrite.Height) !=
          MAPPING_UNKNOWN)
   {
    T6CpuNewerProbeEvidence probe;
    unsigned int sample;

    g_t6CpuNewerProbeRelevant++;
    if (T6CpuNewerProbeForA0(
            VRAMWrite.x, VRAMWrite.y,
            VRAMWrite.Width, VRAMWrite.Height,
            provenanceSeq, &probe))
     {
      g_t6CpuNewerProbeCandidates++;
      if (probe.qualified)
       g_t6CpuNewerProbePasses++;
      sample = g_t6CpuNewerProbeCandidates;
      if (probe.qualified || sample <= 8 ||
          (sample & (sample - 1)) == 0)
       {
        sprintf(txtbuffer,
                "TRB CPU NEWER relevant=%u sample=%u pass=%u "
                "ok=%d tile=%d,%d map=%d/%u seq=%llu "
                "cpu=%llu mat=%llu phys=%llu snap=%llu "
                "result=%d reason=%d hazard=%d cap=%d would=%d "
                "slot=%d capBuf=%d req=%u reqMap=%u reqSeq=%llu\r\n",
                g_t6CpuNewerProbeRelevant, sample,
                g_t6CpuNewerProbePasses, probe.qualified,
                probe.tx, probe.ty, probe.mapping, probe.mapId,
                (unsigned long long)probe.provenanceSeq,
                (unsigned long long)probe.cpuWriteEpoch,
                (unsigned long long)probe.materializedColorEpoch,
                (unsigned long long)probe.physicalEfbSeq,
                (unsigned long long)probe.snapshotSeq,
                (int)probe.result, probe.unresolvedReason,
                probe.hazardTiles, probe.captureRequired,
                probe.wouldCapture, probe.capturePlanSlot,
                probe.captureBufferReady, probe.requiredTiles,
                probe.requiredMapId,
                (unsigned long long)probe.requiredSeq);
        writeLogFile(txtbuffer);
       }
     }
    else if (g_t6CpuNewerProbeRelevant <= 8 ||
             (g_t6CpuNewerProbeRelevant &
              (g_t6CpuNewerProbeRelevant - 1)) == 0)
     {
      sprintf(txtbuffer,
              "TRB CPU NEWER MISS relevant=%u pass=%u seq=%llu "
              "rect=%d,%d %dx%d\r\n",
              g_t6CpuNewerProbeRelevant, g_t6CpuNewerProbePasses,
              (unsigned long long)provenanceSeq,
              VRAMWrite.x, VRAMWrite.y,
              VRAMWrite.Width, VRAMWrite.Height);
      writeLogFile(txtbuffer);
     }
   }
#endif
#if defined(DISP_DEBUG) && defined(VRAM_CONTENT_DIAG)
  if (VRAMWrite.Height >= 120)
   DebugLogVramHalf("A0Done", VRAMWrite.x, VRAMWrite.y,
                    VRAMWrite.Width, VRAMWrite.Height);
#endif
 }

 if(bNeedWriteUpload)
  {
   bNeedWriteUpload=FALSE;
   CheckWriteUpdate();
  }

#if T6_BARRIER_DIAG
 if (g_t6RebuildBaselineCaptures != baselineCapturesBefore)
  g_t6A0CheckOutcomeFlags |= T6_CPU_WRITE_FLAG_BASELINE_ESTABLISHED;
 if (provenanceSeq != 0)
  T6CpuWriteProvenanceUpdate(
      &g_t6CpuWriteProvenance, provenanceSeq,
      g_t6A0CheckOutcomeFlags,
      g_rebuildBaseline.valid ?
          g_rebuildBaseline.capturedContentSeq : 0,
      g_rebuildBaseline.valid ? g_rebuildBaseline.map_id : 0);
 g_t6A0TransferArmed = 0;
#endif

 // set register to NORMAL operation
 bVramWriteTransferActive = FALSE;
 iDataWriteMode = DR_NORMAL;

 // reset transfer values, to prevent mis-transfer of data
 VRAMWrite.ColsRemaining = 0;
 VRAMWrite.RowsRemaining = 0;
}

__inline void FinishedVRAMRead(void)
{
#if T6_BARRIER_DIAG
 if (g_t6C0ActiveSerial != 0)
  {
   if (g_t6C0ActiveSerial <= 4 && g_t6C0LifecycleLogs < 64)
    {
     g_t6C0LifecycleLogs++;
     sprintf(txtbuffer,
             "TRB C0 FINISH serial=%u calls=%u state=%d mode=%d "
             "rem=%d,%d ptr=%u\r\n",
             g_t6C0ActiveSerial, g_t6C0ReadCalls,
             (int)g_readbackState, iDataReadMode,
             VRAMRead.RowsRemaining, VRAMRead.ColsRemaining,
             (unsigned int)(((uintptr_t)VRAMRead.ImagePtr -
                             (uintptr_t)psxVuw) /
                            sizeof(*VRAMRead.ImagePtr)));
     writeLogFile(txtbuffer);
    }
   g_t6C0LastFinishedSerial = g_t6C0ActiveSerial;
   g_t6C0ActiveSerial = 0;
   g_t6C0ReadCalls = 0;
  }
#endif
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

#if T6_BARRIER_DIAG
typedef struct T6C0PerfStats
{
 unsigned int calls;
 unsigned int currentReads;
 unsigned int previousReads;
 unsigned int otherReads;
 unsigned int currentCaptures;
 unsigned int previousCaptures;
 unsigned int slowCalls;
 unsigned int slowLogs;
 uint64_t captureUs;
 uint64_t mergeUs;
 uint64_t invalidateUs;
 uint64_t mergedPixels;
 uint64_t changedPixels;
 uint64_t invalidateRuns;
 uint64_t paletteChecks;
 uint64_t invalidatedEntries;
 unsigned int captureMaxUs;
 unsigned int mergeMaxUs;
 unsigned int invalidateMaxUs;
} T6C0PerfStats;

static T6C0PerfStats g_t6C0Perf;
static unsigned int g_t6PresentFrames;
static unsigned int g_t6PresentCaptures;
static unsigned int g_t6PresentSlow;
static unsigned int g_t6PresentSlowLogs;
static unsigned int g_t6PresentCaptureUsMax;
static uint64_t g_t6PresentCaptureUsTotal;

typedef struct T6FramePerfLast
{
 unsigned int frame;
 unsigned int presentCaptures;
 unsigned int presentSlow;
 unsigned int stdHits;
 unsigned int stdUploads;
 unsigned int wndHits;
 unsigned int wndUploads;
 unsigned int paletteChecks;
 unsigned int invalidatedEntries;
 unsigned int invalidateCalls;
 unsigned int paletteInvalidated;
 unsigned int windowInvalidated;
 unsigned int compressCalls;
 unsigned int barrierCalls;
 unsigned int barrierFastReject;
 unsigned int barrierMaterialized;
 unsigned int barrierFlowCalls;
 unsigned int c0Calls;
 unsigned int c0Captures;
 unsigned int rebuildBaselineCalls;
 unsigned int rebuildBaselineCaptures;
 unsigned int rebuildBaselineFailures;
 uint64_t presentUs;
 uint64_t stdLookupUs;
 uint64_t stdUploadUs;
 uint64_t wndLookupUs;
 uint64_t wndUploadUs;
 uint64_t barrierUs;
 uint64_t barrierFlowUs;
 uint64_t c0CaptureUs;
 uint64_t rebuildBaselineUs;
 uint64_t compressUs;
 uint64_t invalidateUs;
} T6FramePerfLast;

static T6FramePerfLast g_t6FramePerfLast;

static unsigned int T6PerfDeltaU32(unsigned int current,
                                   unsigned int previous)
{
 return current >= previous ? current - previous : current;
}

static uint64_t T6PerfDeltaU64(uint64_t current, uint64_t previous)
{
 return current >= previous ? current - previous : current;
}

static void T6LogFramePerf(void)
{
 unsigned int c0Captures = g_t6C0Perf.currentCaptures +
                           g_t6C0Perf.previousCaptures;
 unsigned int invalidated;
 unsigned int paletteInvalidated;
 unsigned int windowInvalidated;
 unsigned int sourceInvalidated;

 if ((g_t6PresentFrames % 60u) != 0)
  return;

 invalidated = T6PerfDeltaU32(
     g_textureTotalInvalidatedEntries,
     g_t6FramePerfLast.invalidatedEntries);
 paletteInvalidated = T6PerfDeltaU32(
     g_t6PaletteInvalidatedEntries,
     g_t6FramePerfLast.paletteInvalidated);
 windowInvalidated = T6PerfDeltaU32(
     g_t6WindowInvalidatedEntries,
     g_t6FramePerfLast.windowInvalidated);
 sourceInvalidated = invalidated >= paletteInvalidated + windowInvalidated ?
     invalidated - paletteInvalidated - windowInvalidated : 0;

 sprintf(txtbuffer,
         "VRP FRAME frame=%u span=%u presentCap=%u presentUs=%llu "
         "presentMaxUs=%u presentSlow=%u stdLook=%u stdHit=%u stdUp=%u "
         "stdLookupUs=%llu stdLookupMaxUs=%u stdUpUs=%llu stdUpMaxUs=%u "
         "wndLook=%u wndHit=%u wndUp=%u wndLookupUs=%llu "
         "wndLookupMaxUs=%u wndUpUs=%llu wndUpMaxUs=%u "
         "compress=%u compressUs=%llu compressMaxUs=%u "
         "invalCalls=%u invalScanUs=%llu invalScanMaxUs=%u "
         "palChecks=%u invalidated=%u srcInvalid=%u palInvalid=%u "
         "wndInvalid=%u trbCalls=%u trbFast=%u trbMat=%u trbUs=%llu "
         "trbFlowCalls=%u trbFlowUs=%llu trbFlowMaxUs=%u "
         "c0Calls=%u c0Cap=%u c0CapUs=%llu "
         "baseCalls=%u baseCap=%u baseFail=%u baseUs=%llu "
         "baseMaxUs=%u\r\n",
         g_t6PresentFrames,
         T6PerfDeltaU32(g_t6PresentFrames, g_t6FramePerfLast.frame),
         T6PerfDeltaU32(g_t6PresentCaptures,
                        g_t6FramePerfLast.presentCaptures),
         (unsigned long long)T6PerfDeltaU64(
             g_t6PresentCaptureUsTotal, g_t6FramePerfLast.presentUs),
         g_t6PresentCaptureUsMax,
         T6PerfDeltaU32(g_t6PresentSlow, g_t6FramePerfLast.presentSlow),
         T6PerfDeltaU32(g_textureStandardCacheHits +
                        g_textureStandardUploads,
                        g_t6FramePerfLast.stdHits +
                        g_t6FramePerfLast.stdUploads),
         T6PerfDeltaU32(g_textureStandardCacheHits,
                        g_t6FramePerfLast.stdHits),
         T6PerfDeltaU32(g_textureStandardUploads,
                        g_t6FramePerfLast.stdUploads),
         (unsigned long long)T6PerfDeltaU64(
             g_t6StandardLookupUsTotal,
             g_t6FramePerfLast.stdLookupUs),
         g_t6StandardLookupUsMax,
         (unsigned long long)T6PerfDeltaU64(
             g_t6StandardUploadUsTotal,
             g_t6FramePerfLast.stdUploadUs),
         g_t6StandardUploadUsMax,
         T6PerfDeltaU32(g_textureWindowCacheHits + g_textureWindowUploads,
                        g_t6FramePerfLast.wndHits +
                        g_t6FramePerfLast.wndUploads),
         T6PerfDeltaU32(g_textureWindowCacheHits,
                        g_t6FramePerfLast.wndHits),
         T6PerfDeltaU32(g_textureWindowUploads,
                        g_t6FramePerfLast.wndUploads),
         (unsigned long long)T6PerfDeltaU64(
             g_t6WindowLookupUsTotal,
             g_t6FramePerfLast.wndLookupUs),
         g_t6WindowLookupUsMax,
         (unsigned long long)T6PerfDeltaU64(
             g_t6WindowUploadUsTotal,
             g_t6FramePerfLast.wndUploadUs),
         g_t6WindowUploadUsMax,
         T6PerfDeltaU32(g_t6CompressCalls,
                        g_t6FramePerfLast.compressCalls),
         (unsigned long long)T6PerfDeltaU64(
             g_t6CompressUsTotal, g_t6FramePerfLast.compressUs),
         g_t6CompressUsMax,
         T6PerfDeltaU32(g_t6InvalidateCalls,
                        g_t6FramePerfLast.invalidateCalls),
         (unsigned long long)T6PerfDeltaU64(
             g_t6InvalidateUsTotal, g_t6FramePerfLast.invalidateUs),
         g_t6InvalidateUsMax,
         T6PerfDeltaU32(g_texturePaletteEntryChecks,
                        g_t6FramePerfLast.paletteChecks),
         invalidated, sourceInvalidated, paletteInvalidated,
         windowInvalidated,
         T6PerfDeltaU32(g_vramReadBarrierCalls,
                        g_t6FramePerfLast.barrierCalls),
         T6PerfDeltaU32(g_vramReadBarrierFastReject,
                        g_t6FramePerfLast.barrierFastReject),
         T6PerfDeltaU32(g_vramReadBarrierMaterialized,
                        g_t6FramePerfLast.barrierMaterialized),
         (unsigned long long)T6PerfDeltaU64(
             g_vramReadBarrierUsTotal, g_t6FramePerfLast.barrierUs),
         T6PerfDeltaU32(g_t6StandardBarrierFlowCalls,
                        g_t6FramePerfLast.barrierFlowCalls),
         (unsigned long long)T6PerfDeltaU64(
             g_t6StandardBarrierFlowUsTotal,
             g_t6FramePerfLast.barrierFlowUs),
         g_t6StandardBarrierFlowUsMax,
         T6PerfDeltaU32(g_t6C0Perf.calls, g_t6FramePerfLast.c0Calls),
         T6PerfDeltaU32(c0Captures, g_t6FramePerfLast.c0Captures),
         (unsigned long long)T6PerfDeltaU64(
             g_t6C0Perf.captureUs, g_t6FramePerfLast.c0CaptureUs),
         T6PerfDeltaU32(g_t6RebuildBaselineCalls,
                        g_t6FramePerfLast.rebuildBaselineCalls),
         T6PerfDeltaU32(g_t6RebuildBaselineCaptures,
                        g_t6FramePerfLast.rebuildBaselineCaptures),
         T6PerfDeltaU32(g_t6RebuildBaselineFailures,
                        g_t6FramePerfLast.rebuildBaselineFailures),
         (unsigned long long)T6PerfDeltaU64(
             g_t6RebuildBaselineUsTotal,
             g_t6FramePerfLast.rebuildBaselineUs),
         g_t6RebuildBaselineUsMax);
 writeLogFile(txtbuffer);

 g_t6FramePerfLast.frame = g_t6PresentFrames;
 g_t6FramePerfLast.presentCaptures = g_t6PresentCaptures;
 g_t6FramePerfLast.presentSlow = g_t6PresentSlow;
 g_t6FramePerfLast.stdHits = g_textureStandardCacheHits;
 g_t6FramePerfLast.stdUploads = g_textureStandardUploads;
 g_t6FramePerfLast.wndHits = g_textureWindowCacheHits;
 g_t6FramePerfLast.wndUploads = g_textureWindowUploads;
 g_t6FramePerfLast.paletteChecks = g_texturePaletteEntryChecks;
 g_t6FramePerfLast.invalidatedEntries =
     g_textureTotalInvalidatedEntries;
 g_t6FramePerfLast.paletteInvalidated =
     g_t6PaletteInvalidatedEntries;
 g_t6FramePerfLast.windowInvalidated =
     g_t6WindowInvalidatedEntries;
 g_t6FramePerfLast.compressCalls = g_t6CompressCalls;
 g_t6FramePerfLast.invalidateCalls = g_t6InvalidateCalls;
 g_t6FramePerfLast.barrierCalls = g_vramReadBarrierCalls;
 g_t6FramePerfLast.barrierFastReject =
     g_vramReadBarrierFastReject;
 g_t6FramePerfLast.barrierMaterialized =
     g_vramReadBarrierMaterialized;
 g_t6FramePerfLast.barrierFlowCalls =
     g_t6StandardBarrierFlowCalls;
 g_t6FramePerfLast.c0Calls = g_t6C0Perf.calls;
 g_t6FramePerfLast.c0Captures = c0Captures;
 g_t6FramePerfLast.rebuildBaselineCalls =
     g_t6RebuildBaselineCalls;
 g_t6FramePerfLast.rebuildBaselineCaptures =
     g_t6RebuildBaselineCaptures;
 g_t6FramePerfLast.rebuildBaselineFailures =
     g_t6RebuildBaselineFailures;
 g_t6FramePerfLast.presentUs = g_t6PresentCaptureUsTotal;
 g_t6FramePerfLast.stdLookupUs = g_t6StandardLookupUsTotal;
 g_t6FramePerfLast.stdUploadUs = g_t6StandardUploadUsTotal;
 g_t6FramePerfLast.wndLookupUs = g_t6WindowLookupUsTotal;
 g_t6FramePerfLast.wndUploadUs = g_t6WindowUploadUsTotal;
 g_t6FramePerfLast.barrierUs = g_vramReadBarrierUsTotal;
 g_t6FramePerfLast.barrierFlowUs =
     g_t6StandardBarrierFlowUsTotal;
 g_t6FramePerfLast.c0CaptureUs = g_t6C0Perf.captureUs;
 g_t6FramePerfLast.rebuildBaselineUs =
     g_t6RebuildBaselineUsTotal;
 g_t6FramePerfLast.compressUs = g_t6CompressUsTotal;
 g_t6FramePerfLast.invalidateUs = g_t6InvalidateUsTotal;
 g_t6PresentCaptureUsMax = 0;
 g_t6StandardLookupUsMax = 0;
 g_t6StandardUploadUsMax = 0;
 g_t6WindowLookupUsMax = 0;
 g_t6WindowUploadUsMax = 0;
 g_t6CompressUsMax = 0;
 g_t6InvalidateUsMax = 0;
 g_t6StandardBarrierFlowUsMax = 0;
 g_t6RebuildBaselineUsMax = 0;
}

static void T6ProfilePresentedCapture(void)
{
 uint64_t ticksStart = (uint64_t)gettime();
 int captured = CapturePresentedEfbSnapshot();
 unsigned int us = (unsigned int)ticks_to_microsecs(
     (uint64_t)gettime() - ticksStart);

 g_t6PresentFrames++;
 if (captured)
  {
   g_t6PresentCaptures++;
   g_t6PresentCaptureUsTotal += us;
   if (us > g_t6PresentCaptureUsMax)
    g_t6PresentCaptureUsMax = us;
   if (us >= 1000)
    {
     g_t6PresentSlow++;
     if (g_t6PresentSlowLogs < 16)
      {
       g_t6PresentSlowLogs++;
       sprintf(txtbuffer,
               "VRP PRESENT SLOW frame=%u us=%u captures=%u\r\n",
               g_t6PresentFrames, us, g_t6PresentCaptures);
       writeLogFile(txtbuffer);
      }
    }
  }
 T6LogFramePerf();
}

static void T6LogC0Perf(unsigned int callUs, unsigned int captureUs,
                        unsigned int mergeUs, int capturedThisCall)
{
 unsigned int captures = g_t6C0Perf.currentCaptures +
                         g_t6C0Perf.previousCaptures;
 int callMilestone =
     (g_t6C0Perf.calls & (g_t6C0Perf.calls - 1u)) == 0;
 int captureMilestone = capturedThisCall && captures != 0 &&
     (captures & (captures - 1u)) == 0;

 if (callUs >= 1000)
  {
   g_t6C0Perf.slowCalls++;
   if (g_t6C0Perf.slowLogs < 16)
    {
     g_t6C0Perf.slowLogs++;
     sprintf(txtbuffer,
             "VRP SLOW call=%u kind=%d capResult=%d totalUs=%u "
             "capUs=%u mergeUs=%u invalUs=%u merged=%u changed=%u "
             "runs=%u palChecks=%u invalidated=%u\r\n",
             g_t6C0Perf.calls, (int)g_lastReadMapping,
             g_lastCaptureResult, callUs, captureUs, mergeUs,
             g_lastReadbackInvalidateUs, g_lastMergedPixels,
             g_lastMergedChangedPixels, g_lastReadbackInvalidateRuns,
             g_lastReadbackPaletteEntryChecks,
             g_lastReadbackTotalInvalidated);
     writeLogFile(txtbuffer);
    }
  }

 if (!callMilestone && !captureMilestone)
  return;
 sprintf(txtbuffer,
         "VRP C0 calls=%u current=%u previous=%u other=%u "
         "capCurrent=%u capPrevious=%u capUs=%llu capAvgUs=%llu "
         "capMaxUs=%u mergeUs=%llu mergeAvgUs=%llu mergeMaxUs=%u "
         "invalUs=%llu invalMaxUs=%u merged=%llu changed=%llu "
         "runs=%llu palChecks=%llu invalidated=%llu slow=%u\r\n",
         g_t6C0Perf.calls, g_t6C0Perf.currentReads,
         g_t6C0Perf.previousReads, g_t6C0Perf.otherReads,
         g_t6C0Perf.currentCaptures, g_t6C0Perf.previousCaptures,
         (unsigned long long)g_t6C0Perf.captureUs,
         (unsigned long long)(captures ?
             g_t6C0Perf.captureUs / captures : 0),
         g_t6C0Perf.captureMaxUs,
         (unsigned long long)g_t6C0Perf.mergeUs,
         (unsigned long long)(g_t6C0Perf.mergeUs / g_t6C0Perf.calls),
         g_t6C0Perf.mergeMaxUs,
         (unsigned long long)g_t6C0Perf.invalidateUs,
         g_t6C0Perf.invalidateMaxUs,
         (unsigned long long)g_t6C0Perf.mergedPixels,
         (unsigned long long)g_t6C0Perf.changedPixels,
         (unsigned long long)g_t6C0Perf.invalidateRuns,
         (unsigned long long)g_t6C0Perf.paletteChecks,
         (unsigned long long)g_t6C0Perf.invalidatedEntries,
         g_t6C0Perf.slowCalls);
 writeLogFile(txtbuffer);
}
#endif

////////////////////////////////////////////////////////////////////////
// core read from vram
////////////////////////////////////////////////////////////////////////

void CALLBACK GL_GPUreadDataMem(unsigned long * pMem, int iSize)
{
int i;

#ifdef DISP_DEBUG
static unsigned int readCallCount;
readCallCount++;
if (readCallCount <= 4 || iDataReadMode == DR_VRAMTRANSFER ||
    g_readbackState == READBACK_PENDING)
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

#if T6_BARRIER_DIAG
if (g_t6C0ActiveSerial != 0)
 {
  unsigned int call;

  g_t6C0ReadCalls++;
  call = g_t6C0ReadCalls;
  if (g_t6C0ActiveSerial <= 4 && g_t6C0LifecycleLogs < 64 &&
      (call <= 4 || (call & (call - 1u)) == 0))
   {
    g_t6C0LifecycleLogs++;
    sprintf(txtbuffer,
            "TRB C0 PROGRESS serial=%u call=%u size=%d state=%d mode=%d "
            "rem=%d,%d ptr=%u\r\n",
            g_t6C0ActiveSerial, call, iSize,
            (int)g_readbackState, iDataReadMode,
            VRAMRead.RowsRemaining, VRAMRead.ColsRemaining,
            (unsigned int)(((uintptr_t)VRAMRead.ImagePtr -
                            (uintptr_t)psxVuw) /
                           sizeof(*VRAMRead.ImagePtr)));
    writeLogFile(txtbuffer);
   }
 }
#endif

if(iDataReadMode!=DR_VRAMTRANSFER) return;

GPUIsBusy;

if (g_readbackState == READBACK_PENDING)
 {
#if T6_BARRIER_DIAG
  uint64_t callTicksStart = (uint64_t)gettime();
  uint64_t phaseTicksStart;
  unsigned int captureUs;
  unsigned int mergeUs;
  unsigned int callUs;

  g_t6C0Perf.calls++;
#endif
  g_lastReadMapping = ClassifyReadMapping(VRAMRead.x, VRAMRead.y,
                                          VRAMRead.Width, VRAMRead.Height);
#if T6_BARRIER_DIAG
  if (g_lastReadMapping == MAPPING_CURRENT)
   g_t6C0Perf.currentReads++;
  else if (g_lastReadMapping == MAPPING_PREVIOUS)
   g_t6C0Perf.previousReads++;
  else
   g_t6C0Perf.otherReads++;
  phaseTicksStart = (uint64_t)gettime();
#endif
  if (g_lastReadMapping == MAPPING_CURRENT)
   TryCaptureLiveFrame();
  else if (g_lastReadMapping == MAPPING_PREVIOUS &&
           TryCapturePreviousReadRect(VRAMRead.x, VRAMRead.y,
                                      VRAMRead.Width, VRAMRead.Height))
   g_lastCaptureResult = 3;
  else
   g_lastCaptureResult = (g_lastReadMapping == MAPPING_PREVIOUS) ? -5 : -6;
#if T6_BARRIER_DIAG
  captureUs = (unsigned int)ticks_to_microsecs(
      (uint64_t)gettime() - phaseTicksStart);
  if (g_lastCaptureResult == 1)
   g_t6C0Perf.currentCaptures++;
  else if (g_lastCaptureResult == 3)
   g_t6C0Perf.previousCaptures++;
  if (g_lastCaptureResult == 1 || g_lastCaptureResult == 3)
   {
    g_t6C0Perf.captureUs += captureUs;
    if (captureUs > g_t6C0Perf.captureMaxUs)
     g_t6C0Perf.captureMaxUs = captureUs;
   }
  phaseTicksStart = (uint64_t)gettime();
#endif
  MergeReadbackToPsxVuw(VRAMRead.x, VRAMRead.y,
                        VRAMRead.Width, VRAMRead.Height);
#if T6_BARRIER_DIAG
  mergeUs = (unsigned int)ticks_to_microsecs(
      (uint64_t)gettime() - phaseTicksStart);
  if (g_t6C0ActiveSerial != 0 && g_t6C0ActiveSerial <= 4 &&
      g_t6C0LifecycleLogs < 64)
   {
    g_t6C0LifecycleLogs++;
    sprintf(txtbuffer,
            "TRB C0 MERGED serial=%u call=%u kind=%d capResult=%d "
            "mergeUs=%u merged=%u changed=%u rem=%d,%d\r\n",
            g_t6C0ActiveSerial, g_t6C0ReadCalls,
            (int)g_lastReadMapping, g_lastCaptureResult, mergeUs,
            g_lastMergedPixels, g_lastMergedChangedPixels,
            VRAMRead.RowsRemaining, VRAMRead.ColsRemaining);
    writeLogFile(txtbuffer);
   }
  callUs = (unsigned int)ticks_to_microsecs(
      (uint64_t)gettime() - callTicksStart);
  g_t6C0Perf.mergeUs += mergeUs;
  g_t6C0Perf.invalidateUs += g_lastReadbackInvalidateUs;
  g_t6C0Perf.mergedPixels += g_lastMergedPixels;
  g_t6C0Perf.changedPixels += g_lastMergedChangedPixels;
  g_t6C0Perf.invalidateRuns += g_lastReadbackInvalidateRuns;
  g_t6C0Perf.paletteChecks += g_lastReadbackPaletteEntryChecks;
  g_t6C0Perf.invalidatedEntries += g_lastReadbackTotalInvalidated;
  if (mergeUs > g_t6C0Perf.mergeMaxUs)
   g_t6C0Perf.mergeMaxUs = mergeUs;
  if (g_lastReadbackInvalidateUs > g_t6C0Perf.invalidateMaxUs)
   g_t6C0Perf.invalidateMaxUs = g_lastReadbackInvalidateUs;
  T6LogC0Perf(callUs, captureUs, mergeUs,
              g_lastCaptureResult == 1 || g_lastCaptureResult == 3);
#endif
#ifdef DISP_DEBUG
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
#endif
  g_readbackState = READBACK_DONE;
#if T6_BARRIER_DIAG
  if (g_t6C0ActiveSerial != 0 && g_t6C0ActiveSerial <= 4 &&
      g_t6C0LifecycleLogs < 64)
   {
    g_t6C0LifecycleLogs++;
    sprintf(txtbuffer,
            "TRB C0 READY serial=%u call=%u state=%d mode=%d rem=%d,%d\r\n",
            g_t6C0ActiveSerial, g_t6C0ReadCalls,
            (int)g_readbackState, iDataReadMode,
            VRAMRead.RowsRemaining, VRAMRead.ColsRemaining);
    writeLogFile(txtbuffer);
   }
#endif
 }

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

ENDREAD_GL:
GPUIsIdle;
#if T6_BARRIER_DIAG
if (g_t6C0LastFinishedSerial != 0 && g_t6C0ReturnLogs < 4)
 {
  unsigned int finishedSerial = g_t6C0LastFinishedSerial;

  g_t6C0ReturnLogs++;
  g_t6C0LastFinishedSerial = 0;
  g_t6DiagWorkspaceStatus = T6MoveTakeDiagCanaryStatus();
  sprintf(txtbuffer,
          "TRB C0 RETURN serial=%u state=%d mode=%d status=%08lX ws=%d\r\n",
          finishedSerial, (int)g_readbackState, iDataReadMode,
          lGPUstatusRet, g_t6DiagWorkspaceStatus);
  writeLogFile(txtbuffer);
 }
#endif
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

#if T6_BARRIER_DIAG
/* GP1(04h)=2 can select CPU-to-GP0 DMA before the GP0(A0h) header arrives.
 * Count the calls which the descriptor gate keeps in command parsing. */
if(iDataWriteMode==DR_VRAMTRANSFER && !bVramWriteTransferActive)
 {
  g_t6A0UnarmedWriteCalls++;
  if(g_t6A0UnarmedWriteCalls<=8 ||
     (g_t6A0UnarmedWriteCalls & (g_t6A0UnarmedWriteCalls-1))==0)
   {
    sprintf(txtbuffer,
            "TRB A0 GATE call=%u size=%d dma=%u mode=%d active=%d "
            "packet=%ld/%ld rect=%d,%d %dx%d rem=%d,%d\r\n",
            g_t6A0UnarmedWriteCalls, iSize, g_t6A0DmaSerial,
            iDataWriteMode, bVramWriteTransferActive,
            gpuDataP, gpuDataC,
            VRAMWrite.x, VRAMWrite.y, VRAMWrite.Width, VRAMWrite.Height,
            VRAMWrite.RowsRemaining, VRAMWrite.ColsRemaining);
    writeLogFile(txtbuffer);
   }
 }
#endif

/* GP1(04h)=2 selects a DMA direction, not an A0 payload descriptor.  Return
 * to command parsing once, without invoking FinishedVRAMWrite(): there is no
 * transfer whose counters/status/cache side effects could be finished. */
if(T6VramWriteModeNeedsNormalize(
       iDataWriteMode==DR_VRAMTRANSFER, bVramWriteTransferActive))
 iDataWriteMode=DR_NORMAL;

STARTVRAM_GL:

if(T6VramWritePayloadActive(iDataWriteMode==DR_VRAMTRANSFER,
                            bVramWriteTransferActive))
 {
//    #if defined(DISP_DEBUG)
//    sprintf ( txtbuffer, "GPUwriteDataMem DR_VRAMTRANSFER %d \r\n", iSize );
//    writeLogFile(txtbuffer);
//    #endif // DISP_DEBUG

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
 }

ENDVRAM_GL:

if(!T6VramWritePayloadActive(iDataWriteMode==DR_VRAMTRANSFER,
                             bVramWriteTransferActive))
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
    if(T6VramWritePayloadActive(iDataWriteMode==DR_VRAMTRANSFER,
                                bVramWriteTransferActive))
     goto STARTVRAM_GL;

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
       }
      else continue;
     }
    else
     {
       PUTLE32(&gpuDataM[gpuDataP], gdata);
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
      gpuDataC=gpuDataP=0;
      BeginEfbDrawContext();
      primFunc[gpuCommand]((unsigned char *)gpuDataM);
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
    #ifdef DISP_DEBUG
    sprintf(txtbuffer, "flipEGL %d \r\n", canClearFrameBuf);
    DEBUG_print(txtbuffer, DBG_SPU3);
    writeLogFile(txtbuffer);
    #endif // DISP_DEBUG

#if T6_BARRIER_DIAG
    T6ProfilePresentedCapture();
#else
    CapturePresentedEfbSnapshot();
#endif
    AdvanceDC2ReadbackScopeAfterPresent();

    if (canShowFps && showFPSonScreen == 1)
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

    presentSubmitted = gx_vout_render(canClearFrameBuf);

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
