/***************************************************************************
                          gpu.c  -  description
                             -------------------
    begin                : Sun Oct 28 2001
    copyright            : (C) 2001 by Pete Bernert
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
// 2008/05/17 - Pete
// - added GPUvisualVibration and "visual rumble" stuff
//
// 2008/02/03 - Pete
// - added GPUsetframelimit and GPUsetfix ("fake gpu busy states")
//
// 2007/11/03 - Pete
// - new way to create save state picture (Vista)
//
// 2004/01/31 - Pete
// - added zn bits
//
// 2003/01/04 - Pete
// - the odd/even bit hack (CronoCross status screen) is now a special game fix
//
// 2003/01/04 - Pete
// - fixed wrapped y display position offset - Legend of Legaia
//
// 2002/11/24 - Pete
// - added new frameskip func support
//
// 2002/11/02 - Farfetch'd & Pete
// - changed the y display pos handling
//
// 2002/10/03 - Farfetch'd & Pete
// - added all kind of tiny stuff (gpureset, gpugetinfo, dmachain align, polylines...)
//
// 2002/10/03 - Pete
// - fixed gpuwritedatamem & now doing every data processing with it
//
// 2002/08/31 - Pete
// - delayed odd/even toggle for FF8 intro scanlines
//
// 2002/08/03 - Pete
// - "Sprite 1" command count added
//
// 2002/08/03 - Pete
// - handles "screen disable" correctly
//
// 2002/07/28 - Pete
// - changed dmachain handler (monkey hero)
//
// 2002/06/15 - Pete
// - removed dmachain fixes, added dma endless loop detection instead
//
// 2002/05/31 - Lewpy
// - Win95/NT "disable screensaver" fix
//
// 2002/05/30 - Pete
// - dmawrite/read wrap around
//
// 2002/05/15 - Pete
// - Added dmachain "0" check game fix
//
// 2002/04/20 - linuzappz
// - added iFastFwd stuff
//
// 2002/02/18 - linuzappz
// - Added DGA2 support to PIC stuff
//
// 2002/02/10 - Pete
// - Added dmacheck for The Mummy and T'ai Fu
//
// 2002/01/13 - linuzappz
// - Added timing in the GPUdisplayText func
//
// 2002/01/06 - lu
// - Added some #ifdef for the linux configurator
//
// 2002/01/05 - Pete
// - fixed unwanted screen clearing on horizontal centering (causing
//   flickering in linux version)
//
// 2001/12/10 - Pete
// - fix for Grandia in ChangeDispOffsetsX
//
// 2001/12/05 - syo (syo68k@geocities.co.jp)
// - added disable screen saver for "stop screen saver" option
//
// 2001/11/20 - linuzappz
// - added Soft and About DlgProc calls in GPUconfigure and
//   GPUabout, for linux
//
// 2001/11/09 - Darko Matesic
// - added recording frame in updateLace and stop recording
//   in GPUclose (if it is still recording)
//
// 2001/10/28 - Pete
// - generic cleanup for the Peops release
//
//*************************************************************************//


#include "stdafx.h"
#include <stdbool.h>
#include "../Gamecube/DEBUG.h"
#include "../Gamecube/wiiSXconfig.h"

#define _IN_GPU

#include "externals.h"
#include "gpu.h"
#include "draw.h"
#include "cfg.h"
#include "prim.h"
#include "psemu.h"
#include "menu.h"
#include "key.h"
#include "fps.h"
#include "swap.h"

//#define SMALLDEBUG
//#include <dbgout.h>


////////////////////////////////////////////////////////////////////////
// PPDK developer must change libraryName field and can change revision and build
////////////////////////////////////////////////////////////////////////

const  unsigned char version  = 1;    // do not touch - library for PSEmu 1.x
const  unsigned char revision = 1;
const  unsigned char build    = 18;   // increase that with each version



static char *libraryName      = "P.E.Op.S. SoftGPU Driver";
static char *libraryInfo      = "P.E.Op.S. SoftGPU Driver V1.18\nCoded by Pete Bernert and the P.E.Op.S. team\n";

static char *PluginAuthor     = "Pete Bernert and the P.E.Op.S. team";

////////////////////////////////////////////////////////////////////////
// memory image of the PSX vram
////////////////////////////////////////////////////////////////////////

unsigned char  psxVSecure[(iGPUHeight*2)*1024 + (1024*1024)];
unsigned char  *psxVub;
signed   char  *psxVsb;
unsigned short *psxVuw;
unsigned short *psxVuw_eom;
signed   short *psxVsw;
unsigned long  *psxVul;
signed   long  *psxVsl;

////////////////////////////////////////////////////////////////////////
// GPU globals
////////////////////////////////////////////////////////////////////////

static long       lGPUdataRet;
long              lGPUstatusRet;
char              szDispBuf[64];
char              szMenuBuf[36];
char              szDebugText[512];
unsigned long     ulStatusControl[256];

static unsigned   long gpuDataM[256];
static unsigned   char gpuCommand = 0;
static long       gpuDataC = 0;
static long       gpuDataP = 0;

VRAMLoad_t        VRAMWrite;
VRAMLoad_t        VRAMRead;
DATAREGISTERMODES DataWriteMode;
DATAREGISTERMODES DataReadMode;

BOOL              bSkipNextFrame = FALSE;
DWORD             dwLaceCnt=0;
int               iColDepth;
int               iWindowMode;
short             sDispWidths[8] = {256,320,512,640,368,384,512,640};
PSXDisplay_t      PSXDisplay;
PSXDisplay_t      PreviousPSXDisplay;
long              lSelectedSlot=0;
BOOL              bChangeWinMode=FALSE;
BOOL              bDoLazyUpdate=FALSE;
unsigned long     lGPUInfoVals[16];
int               iFakePrimBusy=0;
int               iRumbleVal=0;
int               iRumbleTime=0;

////////////////////////////////////////////////////////////////////////
// some misc external display funcs
////////////////////////////////////////////////////////////////////////

/*
unsigned long PCADDR;
void CALLBACK GPUdebugSetPC(unsigned long addr)
{
 PCADDR=addr;
}
*/

#include <time.h>
time_t tStart;

void PEOPS_GPUdisplayText(char * pText)             // some debug func
{
 if(!pText) {szDebugText[0]=0;return;}
 if(strlen(pText)>511) return;
 time(&tStart);
 strcpy(szDebugText,pText);
}

////////////////////////////////////////////////////////////////////////

void CALLBACK GPUdisplayFlags(unsigned long dwFlags)   // some info func
{
 dwCoreFlags=dwFlags;
 BuildDispMenu(0);
}

////////////////////////////////////////////////////////////////////////
// stuff to make this a true PDK module
////////////////////////////////////////////////////////////////////////

char * CALLBACK PSEgetLibName(void)
{
 return libraryName;
}

unsigned long CALLBACK PSEgetLibType(void)
{
 return  PSE_LT_GPU;
}

unsigned long CALLBACK PSEgetLibVersion(void)
{
 return version<<16|revision<<8|build;
}

char * GPUgetLibInfos(void)
{
 return libraryInfo;
}

////////////////////////////////////////////////////////////////////////
// Snapshot func
////////////////////////////////////////////////////////////////////////

char * pGetConfigInfos(int iCfg)
{
 char szO[2][4]={"off","on "};
 char szTxt[256];
 char * pB=(char *)malloc(32767);

 if(!pB) return NULL;
 *pB=0;
 //----------------------------------------------------//
 sprintf(szTxt,"Plugin: %s %d.%d.%d\r\n",libraryName,version,revision,build);
 strcat(pB,szTxt);
 sprintf(szTxt,"Author: %s\r\n\r\n",PluginAuthor);
 strcat(pB,szTxt);
 //----------------------------------------------------//
 if(iCfg && iWindowMode)
  sprintf(szTxt,"Resolution/Color:\r\n- %ux%u ",LOWORD(iWinSize),HIWORD(iWinSize));
 else
  sprintf(szTxt,"Resolution/Color:\r\n- %dx%d ",iResX,iResY);
 strcat(pB,szTxt);
 if(iWindowMode && iCfg)
   strcpy(szTxt,"Window mode\r\n");
 else
 if(iWindowMode)
   sprintf(szTxt,"Window mode - [%d Bit]\r\n",iDesktopCol);
 else
   sprintf(szTxt,"Fullscreen - [%d Bit]\r\n",iColDepth);
 strcat(pB,szTxt);

 sprintf(szTxt,"Stretch mode: %d\r\n",iUseNoStretchBlt);
 strcat(pB,szTxt);
 sprintf(szTxt,"Dither mode: %d\r\n\r\n",iUseDither);
 strcat(pB,szTxt);
 //----------------------------------------------------//
 sprintf(szTxt,"Framerate:\r\n- FPS limit: %s\r\n",szO[UseFrameLimit]);
 strcat(pB,szTxt);
 sprintf(szTxt,"- Frame skipping: %s",szO[UseFrameSkip]);
 strcat(pB,szTxt);
 if(iFastFwd) strcat(pB," (fast forward)");
 strcat(pB,"\r\n");
 if(iFrameLimit==2)
      strcpy(szTxt,"- FPS limit: Auto\r\n\r\n");
 else sprintf(szTxt,"- FPS limit: %.1f\r\n\r\n",fFrameRate);
 strcat(pB,szTxt);
 //----------------------------------------------------//
 strcpy(szTxt,"Misc:\r\n- Scanlines: ");
 if(iUseScanLines==0) strcat(szTxt,"disabled");
 else
 if(iUseScanLines==1) strcat(szTxt,"standard");
 else
 if(iUseScanLines==2) strcat(szTxt,"double blitting");
 strcat(szTxt,"\r\n");
 strcat(pB,szTxt);
 sprintf(szTxt,"- Game fixes: %s [%08lx]\r\n",szO[iUseFixes],dwCfgFixes);
 strcat(pB,szTxt);
 //----------------------------------------------------//
 return pB;
}

void DoTextSnapShot(int iNum)
{
 FILE *txtfile;char szTxt[256];char * pB;

 sprintf(szTxt,"%s/peopssoft%03d.txt",getenv("HOME"),iNum);

 if((txtfile=fopen(szTxt,"wb"))==NULL)
  return;
 //----------------------------------------------------//
 pB=pGetConfigInfos(0);
 if(pB)
  {
   fwrite(pB,strlen(pB),1,txtfile);
   free(pB);
  }
 fclose(txtfile);
}

////////////////////////////////////////////////////////////////////////

void CALLBACK GPUmakeSnapshot(void)                    // snapshot of whole vram
{
 FILE *bmpfile;
 char filename[256];
 unsigned char header[0x36];
 long size,height;
 unsigned char line[1024*3];
 short i,j;
 unsigned char empty[2]={0,0};
 unsigned short color;
 unsigned long snapshotnr = 0;

 height=iGPUHeight;

 size=height*1024*3+0x38;

 // fill in proper values for BMP

 // hardcoded BMP header
 memset(header,0,0x36);
 header[0]='B';
 header[1]='M';
 header[2]=size&0xff;
 header[3]=(size>>8)&0xff;
 header[4]=(size>>16)&0xff;
 header[5]=(size>>24)&0xff;
 header[0x0a]=0x36;
 header[0x0e]=0x28;
 header[0x12]=1024%256;
 header[0x13]=1024/256;
 header[0x16]=height%256;
 header[0x17]=height/256;
 header[0x1a]=0x01;
 header[0x1c]=0x18;
 header[0x26]=0x12;
 header[0x27]=0x0B;
 header[0x2A]=0x12;
 header[0x2B]=0x0B;

 // increment snapshot value & try to get filename
 do
  {
   snapshotnr++;
   sprintf(filename,"%s/peopssoft%03lu.bmp",getenv("HOME"),snapshotnr);

   bmpfile=fopen(filename,"rb");
   if (bmpfile == NULL) break;
   fclose(bmpfile);
  }
 while(TRUE);

 // try opening new snapshot file
 if((bmpfile=fopen(filename,"wb"))==NULL)
  return;

 fwrite(header,0x36,1,bmpfile);
 for(i=height-1;i>=0;i--)
  {
   for(j=0;j<1024;j++)
    {
     color=psxVuw[i*1024+j];
     line[j*3+2]=(color<<3)&0xf1;
     line[j*3+1]=(color>>2)&0xf1;
     line[j*3+0]=(color>>7)&0xf1;
    }
   fwrite(line,1024*3,1,bmpfile);
  }
 fwrite(empty,0x2,1,bmpfile);
 fclose(bmpfile);

 DoTextSnapShot(snapshotnr);
}

////////////////////////////////////////////////////////////////////////
// INIT, will be called after lib load... well, just do some var init...
////////////////////////////////////////////////////////////////////////

long PEOPS_GPUinit()                                // GPU INIT
{
 memset(ulStatusControl,0,256*sizeof(unsigned long));  // init save state scontrol field

 szDebugText[0]=0;                                     // init debug text buffer

 //psxVSecure=(unsigned char *)malloc((iGPUHeight*2)*1024 + (1024*1024)); // always alloc one extra MB for soft drawing funcs security
 //if(!psxVSecure) return -1;

 //!!! ATTENTION !!!
 psxVub=psxVSecure+512*1024;                           // security offset into double sized psx vram!

 psxVsb=(signed char *)psxVub;                         // different ways of accessing PSX VRAM
 psxVsw=(signed short *)psxVub;
 psxVsl=(signed long *)psxVub;
 psxVuw=(unsigned short *)psxVub;
 psxVul=(unsigned long *)psxVub;

 psxVuw_eom=psxVuw+1024*iGPUHeight;                    // pre-calc of end of vram

 memset(psxVSecure,0x00,(iGPUHeight*2)*1024 + (1024*1024));
 memset(lGPUInfoVals,0x00,16*sizeof(unsigned long));

 SetFPSHandler();

 PSXDisplay.RGB24        = FALSE;                      // init some stuff
 PSXDisplay.Interlaced   = FALSE;
 PSXDisplay.DrawOffset.x = 0;
 PSXDisplay.DrawOffset.y = 0;
 PSXDisplay.DisplayMode.x= 320;
 PSXDisplay.DisplayMode.y= 240;
 PreviousPSXDisplay.DisplayMode.x= 320;
 PreviousPSXDisplay.DisplayMode.y= 240;
 PSXDisplay.Disabled     = FALSE;
 PreviousPSXDisplay.Range.x0 =0;
 PreviousPSXDisplay.Range.y0 =0;
 PSXDisplay.Range.x0=0;
 PSXDisplay.Range.x1=0;
 PreviousPSXDisplay.DisplayModeNew.y=0;
 PSXDisplay.Double=1;
 lGPUdataRet=0x400;

 DataWriteMode = DR_NORMAL;

 // Reset transfer values, to prevent mis-transfer of data
 memset(&VRAMWrite,0,sizeof(VRAMLoad_t));
 memset(&VRAMRead,0,sizeof(VRAMLoad_t));

 // device initialised already !
 lGPUstatusRet = 0x14802000;
 GPUIsIdle;
 GPUIsReadyForCommands;
 bDoVSyncUpdate=TRUE;

 return 0;
}

////////////////////////////////////////////////////////////////////////
// Here starts all...
////////////////////////////////////////////////////////////////////////

long PEOPS_GPUopen(unsigned long * disp,char * CapText,char * CfgFile)
{
 unsigned long d;

 pCaptionText=CapText;

#ifndef _FPSE
 pConfigFile=CfgFile;
#endif

 ReadConfig();                                         // read registry

 iShowFPS=1;	//Default config turns this off..

 InitFPS();

 bIsFirstFrame  = TRUE;                                // we have to init later
 bDoVSyncUpdate = TRUE;

 d=ulInitDisplay();                                    // setup x

 if(disp) *disp=d;                                     // wanna x pointer? ok

 if(d) return 0;
 return -1;
}

////////////////////////////////////////////////////////////////////////
// time to leave...
////////////////////////////////////////////////////////////////////////

long PEOPS_GPUclose()
{

 ReleaseKeyHandler();                                  // de-subclass window

 CloseDisplay();                                       // shutdown direct draw

 return 0;
}

////////////////////////////////////////////////////////////////////////
// I shot the sheriff
////////////////////////////////////////////////////////////////////////

long PEOPS_GPUshutdown()
{

// free(psxVSecure);

 return 0;                                             // nothinh to do
}

////////////////////////////////////////////////////////////////////////
// Update display (swap buffers)
////////////////////////////////////////////////////////////////////////

void updateDisplay(void)                               // UPDATE DISPLAY
{
 if(PSXDisplay.Disabled)                               // disable?
  {
   DoClearFrontBuffer();                               // -> clear frontbuffer
   return;                                             // -> and bye
  }

 if(dwActFixes&32)                                     // pc fps calculation fix
  {
   if(UseFrameLimit) PCFrameCap();                     // -> brake
   if(UseFrameSkip || ulKeybits&KEY_SHOWFPS)
    PCcalcfps();
  }

 if(ulKeybits&KEY_SHOWFPS)                             // make fps display buf
  {
   sprintf(szDispBuf,"FPS %06.2f",fps_cur);
  }

 if(iFastFwd)                                          // fastfwd ?
  {
   static int fpscount; UseFrameSkip=1;

   if(!bSkipNextFrame) DoBufferSwap();                 // -> to skip or not to skip
   if(fpscount%6)                                      // -> skip 6/7 frames
        bSkipNextFrame = TRUE;
   else bSkipNextFrame = FALSE;
   fpscount++;
   if(fpscount >= (int)fFrameRateHz) fpscount = 0;
   return;
  }

 if(UseFrameSkip)                                      // skip ?
  {
   if(!bSkipNextFrame) DoBufferSwap();                 // -> to skip or not to skip
   if(dwActFixes&0xa0)                                 // -> pc fps calculation fix/old skipping fix
    {
     if((fps_skip < fFrameRateHz) && !(bSkipNextFrame))  // -> skip max one in a row
         {bSkipNextFrame = TRUE; fps_skip=fFrameRateHz;}
     else bSkipNextFrame = FALSE;
    }
   else FrameSkip();
  }
 else                                                  // no skip ?
  {
   DoBufferSwap();                                     // -> swap
  }
}

////////////////////////////////////////////////////////////////////////
// roughly emulated screen centering bits... not complete !!!
////////////////////////////////////////////////////////////////////////

void ChangeDispOffsetsX(void)                          // X CENTER
{
 long lx,l;

 if(!PSXDisplay.Range.x1) return;

 l=PreviousPSXDisplay.DisplayMode.x;

 l*=(long)PSXDisplay.Range.x1;
 l/=2560;lx=l;l&=0xfffffff8;

 if(l==PreviousPSXDisplay.Range.y1) return;            // abusing range.y1 for
 PreviousPSXDisplay.Range.y1=(short)l;                 // storing last x range and test

 if(lx>=PreviousPSXDisplay.DisplayMode.x)
  {
   PreviousPSXDisplay.Range.x1=
    (short)PreviousPSXDisplay.DisplayMode.x;
   PreviousPSXDisplay.Range.x0=0;
  }
 else
  {
   PreviousPSXDisplay.Range.x1=(short)l;

   PreviousPSXDisplay.Range.x0=
    (PSXDisplay.Range.x0-500)/8;

   if(PreviousPSXDisplay.Range.x0<0)
    PreviousPSXDisplay.Range.x0=0;

   if((PreviousPSXDisplay.Range.x0+lx)>
      PreviousPSXDisplay.DisplayMode.x)
    {
     PreviousPSXDisplay.Range.x0=
      (short)(PreviousPSXDisplay.DisplayMode.x-lx);
     PreviousPSXDisplay.Range.x0+=2; //???

     PreviousPSXDisplay.Range.x1+=(short)(lx-l);
     PreviousPSXDisplay.Range.x1-=2; // makes stretching easier
    }

   // some alignment security
   PreviousPSXDisplay.Range.x0=PreviousPSXDisplay.Range.x0>>1;
   PreviousPSXDisplay.Range.x0=PreviousPSXDisplay.Range.x0<<1;
   PreviousPSXDisplay.Range.x1=PreviousPSXDisplay.Range.x1>>1;
   PreviousPSXDisplay.Range.x1=PreviousPSXDisplay.Range.x1<<1;

   //DoClearScreenBuffer();
  }

 bDoVSyncUpdate=TRUE;
}

////////////////////////////////////////////////////////////////////////

void ChangeDispOffsetsY(void)                          // Y CENTER
{
 int iT,iO=PreviousPSXDisplay.Range.y0;
 int iOldYOffset=PreviousPSXDisplay.DisplayModeNew.y;

// new

 if((PreviousPSXDisplay.DisplayModeNew.x+PSXDisplay.DisplayModeNew.y)>iGPUHeight)
  {
   int dy1=iGPUHeight-PreviousPSXDisplay.DisplayModeNew.x;
   int dy2=(PreviousPSXDisplay.DisplayModeNew.x+PSXDisplay.DisplayModeNew.y)-iGPUHeight;

   if(dy1>=dy2)
    {
     PreviousPSXDisplay.DisplayModeNew.y=-dy2;
    }
   else
    {
     PSXDisplay.DisplayPosition.y=0;
     PreviousPSXDisplay.DisplayModeNew.y=-dy1;
    }
  }
 else PreviousPSXDisplay.DisplayModeNew.y=0;

// eon

 if(PreviousPSXDisplay.DisplayModeNew.y!=iOldYOffset) // if old offset!=new offset: recalc height
  {
   PSXDisplay.Height = PSXDisplay.Range.y1 -
                       PSXDisplay.Range.y0 +
                       PreviousPSXDisplay.DisplayModeNew.y;
   PSXDisplay.DisplayModeNew.y=PSXDisplay.Height*PSXDisplay.Double;
  }

//

 if(PSXDisplay.PAL) iT=48; else iT=28;

 if(PSXDisplay.Range.y0>=iT)
  {
   PreviousPSXDisplay.Range.y0=
    (short)((PSXDisplay.Range.y0-iT-4)*PSXDisplay.Double);
   if(PreviousPSXDisplay.Range.y0<0)
    PreviousPSXDisplay.Range.y0=0;
   PSXDisplay.DisplayModeNew.y+=
    PreviousPSXDisplay.Range.y0;
  }
 else
  PreviousPSXDisplay.Range.y0=0;

// if(iO!=PreviousPSXDisplay.Range.y0)
//  {
//   DoClearScreenBuffer();
// }
}

////////////////////////////////////////////////////////////////////////
// check if update needed
////////////////////////////////////////////////////////////////////////

void updateDisplayIfChanged(void)                      // UPDATE DISPLAY IF CHANGED
{
 if ((PSXDisplay.DisplayMode.y == PSXDisplay.DisplayModeNew.y) &&
     (PSXDisplay.DisplayMode.x == PSXDisplay.DisplayModeNew.x))
  {
   if((PSXDisplay.RGB24      == PSXDisplay.RGB24New) &&
      (PSXDisplay.Interlaced == PSXDisplay.InterlacedNew)) return;
  }

 PSXDisplay.RGB24         = PSXDisplay.RGB24New;       // get new infos

 PSXDisplay.DisplayMode.y = PSXDisplay.DisplayModeNew.y;
 PSXDisplay.DisplayMode.x = PSXDisplay.DisplayModeNew.x;
 PreviousPSXDisplay.DisplayMode.x=                     // previous will hold
  min(640,PSXDisplay.DisplayMode.x);                   // max 640x512... that's
 PreviousPSXDisplay.DisplayMode.y=                     // the size of my
  min(512,PSXDisplay.DisplayMode.y);                   // back buffer surface
 PSXDisplay.Interlaced    = PSXDisplay.InterlacedNew;

 PSXDisplay.DisplayEnd.x=                              // calc end of display
  PSXDisplay.DisplayPosition.x+ PSXDisplay.DisplayMode.x;
 PSXDisplay.DisplayEnd.y=
  PSXDisplay.DisplayPosition.y+ PSXDisplay.DisplayMode.y+PreviousPSXDisplay.DisplayModeNew.y;
 PreviousPSXDisplay.DisplayEnd.x=
  PreviousPSXDisplay.DisplayPosition.x+ PSXDisplay.DisplayMode.x;
 PreviousPSXDisplay.DisplayEnd.y=
  PreviousPSXDisplay.DisplayPosition.y+ PSXDisplay.DisplayMode.y+PreviousPSXDisplay.DisplayModeNew.y;

 ChangeDispOffsetsX();

 if(iFrameLimit==2) SetAutoFrameCap();                 // -> set it

 if(UseFrameSkip) updateDisplay();                     // stupid stuff when frame skipping enabled
}

////////////////////////////////////////////////////////////////////////
// gun cursor func: player=0-7, x=0-511, y=0-255
////////////////////////////////////////////////////////////////////////

void CALLBACK GPUcursor(int iPlayer,int x,int y)
{
 if(iPlayer<0) return;
 if(iPlayer>7) return;

 usCursorActive|=(1<<iPlayer);

 if(x<0)       x=0;
 if(x>511)     x=511;
 if(y<0)       y=0;
 if(y>255)     y=255;

 ptCursorPoint[iPlayer].x=x;
 ptCursorPoint[iPlayer].y=y;
}

////////////////////////////////////////////////////////////////////////
// update lace is called every VSync
////////////////////////////////////////////////////////////////////////

void PEOPS_GPUupdateLace(void)
{
#ifdef PEOPS_SDLOG
	DEBUG_print("append",DBG_SDGECKOAPPEND);
	sprintf(txtbuffer,"Calling GPUupdateLace()\r\n");
	DEBUG_print(txtbuffer,DBG_SDGECKOPRINT);
	DEBUG_print("close",DBG_SDGECKOCLOSE);
#endif //PEOPS_SDLOG
 if(!(dwActFixes&1))
  lGPUstatusRet^=0x80000000;                           // odd/even bit

 if(!(dwActFixes&32))                                  // std fps limitation?
  CheckFrameRate();

 if(PSXDisplay.Interlaced)                             // interlaced mode?
  {
   if(bDoVSyncUpdate && PSXDisplay.DisplayMode.x>0 && PSXDisplay.DisplayMode.y>0)
    {
     updateDisplay();
    }
  }
 else                                                  // non-interlaced?
  {
   if(dwActFixes&64)                                   // lazy screen update fix
    {
     if(bDoLazyUpdate && !UseFrameSkip)
      updateDisplay();
     bDoLazyUpdate=FALSE;
    }
   else
    {
     if(bDoVSyncUpdate && !UseFrameSkip)               // some primitives drawn?
      updateDisplay();                                 // -> update display
    }
  }

 bDoVSyncUpdate=FALSE;                                 // vsync done
}

////////////////////////////////////////////////////////////////////////
// process read request from GPU status register
////////////////////////////////////////////////////////////////////////

unsigned long PEOPS_GPUreadStatus(void)
{
 if(dwActFixes&1)
  {
   static int iNumRead=0;                              // odd/even hack
   if((iNumRead++)==2)
    {
     iNumRead=0;
     lGPUstatusRet^=0x80000000;                        // interlaced bit toggle... we do it on every 3 read status... needed by some games (like ChronoCross) with old epsxe versions (1.5.2 and older)
    }
  }

// if(GetAsyncKeyState(VK_SHIFT)&32768) auxprintf("1 %08x\n",lGPUstatusRet);

 if(iFakePrimBusy)                                     // 27.10.2007 - PETE : emulating some 'busy' while drawing... pfff
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
//   auxprintf("2 %08x\n",lGPUstatusRet);
  }

 return lGPUstatusRet;
}

////////////////////////////////////////////////////////////////////////
// processes data send to GPU status register
// these are always single packet commands.
////////////////////////////////////////////////////////////////////////

void PEOPS_GPUwriteStatus(unsigned long gdata)
{
 unsigned long lCommand=(gdata>>24)&0xff;

 ulStatusControl[lCommand]=gdata;                      // store command for freezing

 switch(lCommand)
  {
   //--------------------------------------------------//
   // reset gpu
   case 0x00:
    memset(lGPUInfoVals,0x00,16*sizeof(unsigned long));
    lGPUstatusRet=0x14802000;
    PSXDisplay.Disabled=1;
    DataWriteMode=DataReadMode=DR_NORMAL;
    PSXDisplay.DrawOffset.x=PSXDisplay.DrawOffset.y=0;
    drawX=drawY=0;drawW=drawH=0;
    sSetMask=0;lSetMask=0;bCheckMask=FALSE;
    usMirror=0;
    GlobalTextAddrX=0;GlobalTextAddrY=0;
    GlobalTextTP=0;GlobalTextABR=0;
    PSXDisplay.RGB24=FALSE;
    PSXDisplay.Interlaced=FALSE;
    bUsingTWin = FALSE;
    return;
   //--------------------------------------------------//
   // dis/enable display
   case 0x03:

    PreviousPSXDisplay.Disabled = PSXDisplay.Disabled;
    PSXDisplay.Disabled = (gdata & 1);

    if(PSXDisplay.Disabled)
         lGPUstatusRet|=GPUSTATUS_DISPLAYDISABLED;
    else lGPUstatusRet&=~GPUSTATUS_DISPLAYDISABLED;
    return;

   //--------------------------------------------------//
   // setting transfer mode
   case 0x04:
    gdata &= 0x03;                                     // Only want the lower two bits

    DataWriteMode=DataReadMode=DR_NORMAL;
    if(gdata==0x02) DataWriteMode=DR_VRAMTRANSFER;
    if(gdata==0x03) DataReadMode =DR_VRAMTRANSFER;
    lGPUstatusRet&=~GPUSTATUS_DMABITS;                 // Clear the current settings of the DMA bits
    lGPUstatusRet|=(gdata << 29);                      // Set the DMA bits according to the received data

    return;
   //--------------------------------------------------//
   // setting display position
   case 0x05:
    {
     PreviousPSXDisplay.DisplayPosition.x = PSXDisplay.DisplayPosition.x;
     PreviousPSXDisplay.DisplayPosition.y = PSXDisplay.DisplayPosition.y;

////////
/*
     PSXDisplay.DisplayPosition.y = (short)((gdata>>10)&0x3ff);
     if (PSXDisplay.DisplayPosition.y & 0x200)
      PSXDisplay.DisplayPosition.y |= 0xfffffc00;
     if(PSXDisplay.DisplayPosition.y<0)
      {
       PreviousPSXDisplay.DisplayModeNew.y=PSXDisplay.DisplayPosition.y/PSXDisplay.Double;
       PSXDisplay.DisplayPosition.y=0;
      }
     else PreviousPSXDisplay.DisplayModeNew.y=0;
*/

// new
     if(iGPUHeight==1024)
      {
       if(dwGPUVersion==2)
            PSXDisplay.DisplayPosition.y = (short)((gdata>>12)&0x3ff);
       else PSXDisplay.DisplayPosition.y = (short)((gdata>>10)&0x3ff);
      }
     else PSXDisplay.DisplayPosition.y = (short)((gdata>>10)&0x1ff);

     // store the same val in some helper var, we need it on later compares
     PreviousPSXDisplay.DisplayModeNew.x=PSXDisplay.DisplayPosition.y;

     if((PSXDisplay.DisplayPosition.y+PSXDisplay.DisplayMode.y)>iGPUHeight)
      {
       int dy1=iGPUHeight-PSXDisplay.DisplayPosition.y;
       int dy2=(PSXDisplay.DisplayPosition.y+PSXDisplay.DisplayMode.y)-iGPUHeight;

       if(dy1>=dy2)
        {
         PreviousPSXDisplay.DisplayModeNew.y=-dy2;
        }
       else
        {
         PSXDisplay.DisplayPosition.y=0;
         PreviousPSXDisplay.DisplayModeNew.y=-dy1;
        }
      }
     else PreviousPSXDisplay.DisplayModeNew.y=0;
// eon

     PSXDisplay.DisplayPosition.x = (short)(gdata & 0x3ff);
     PSXDisplay.DisplayEnd.x=
      PSXDisplay.DisplayPosition.x+ PSXDisplay.DisplayMode.x;
     PSXDisplay.DisplayEnd.y=
      PSXDisplay.DisplayPosition.y+ PSXDisplay.DisplayMode.y + PreviousPSXDisplay.DisplayModeNew.y;
     PreviousPSXDisplay.DisplayEnd.x=
      PreviousPSXDisplay.DisplayPosition.x+ PSXDisplay.DisplayMode.x;
     PreviousPSXDisplay.DisplayEnd.y=
      PreviousPSXDisplay.DisplayPosition.y+ PSXDisplay.DisplayMode.y + PreviousPSXDisplay.DisplayModeNew.y;

     bDoVSyncUpdate=TRUE;

     if (!(PSXDisplay.Interlaced))                      // stupid frame skipping option
      {
       if(UseFrameSkip)  updateDisplay();
       if(dwActFixes&64) bDoLazyUpdate=TRUE;
      }
    }return;
   //--------------------------------------------------//
   // setting width
   case 0x06:

    PSXDisplay.Range.x0=(short)(gdata & 0x7ff);
    PSXDisplay.Range.x1=(short)((gdata>>12) & 0xfff);

    PSXDisplay.Range.x1-=PSXDisplay.Range.x0;

    ChangeDispOffsetsX();

    return;
   //--------------------------------------------------//
   // setting height
   case 0x07:
    {

     PSXDisplay.Range.y0=(short)(gdata & 0x3ff);
     PSXDisplay.Range.y1=(short)((gdata>>10) & 0x3ff);

     PreviousPSXDisplay.Height = PSXDisplay.Height;

     PSXDisplay.Height = PSXDisplay.Range.y1 -
                         PSXDisplay.Range.y0 +
                         PreviousPSXDisplay.DisplayModeNew.y;

     if(PreviousPSXDisplay.Height!=PSXDisplay.Height)
      {
       PSXDisplay.DisplayModeNew.y=PSXDisplay.Height*PSXDisplay.Double;

       ChangeDispOffsetsY();

       updateDisplayIfChanged();
      }
     return;
    }
   //--------------------------------------------------//
   // setting display infos
   case 0x08:

    PSXDisplay.DisplayModeNew.x =
     sDispWidths[(gdata & 0x03) | ((gdata & 0x40) >> 4)];

    if (gdata&0x04) PSXDisplay.Double=2;
    else            PSXDisplay.Double=1;

    PSXDisplay.DisplayModeNew.y = PSXDisplay.Height*PSXDisplay.Double;

    ChangeDispOffsetsY();

    PSXDisplay.PAL           = (gdata & 0x08)?TRUE:FALSE; // if 1 - PAL mode, else NTSC
    PSXDisplay.RGB24New      = (gdata & 0x10)?TRUE:FALSE; // if 1 - TrueColor
    PSXDisplay.InterlacedNew = (gdata & 0x20)?TRUE:FALSE; // if 1 - Interlace

    lGPUstatusRet&=~GPUSTATUS_WIDTHBITS;                   // Clear the width bits
    lGPUstatusRet|=
               (((gdata & 0x03) << 17) |
               ((gdata & 0x40) << 10));                // Set the width bits

    if(PSXDisplay.InterlacedNew)
     {
      if(!PSXDisplay.Interlaced)
       {
        PreviousPSXDisplay.DisplayPosition.x = PSXDisplay.DisplayPosition.x;
        PreviousPSXDisplay.DisplayPosition.y = PSXDisplay.DisplayPosition.y;
       }
      lGPUstatusRet|=GPUSTATUS_INTERLACED;
     }
    else lGPUstatusRet&=~GPUSTATUS_INTERLACED;

    if (PSXDisplay.PAL)
         lGPUstatusRet|=GPUSTATUS_PAL;
    else lGPUstatusRet&=~GPUSTATUS_PAL;

    if (PSXDisplay.Double==2)
         lGPUstatusRet|=GPUSTATUS_DOUBLEHEIGHT;
    else lGPUstatusRet&=~GPUSTATUS_DOUBLEHEIGHT;

    if (PSXDisplay.RGB24New)
         lGPUstatusRet|=GPUSTATUS_RGB24;
    else lGPUstatusRet&=~GPUSTATUS_RGB24;

    updateDisplayIfChanged();

    return;
   //--------------------------------------------------//
   // ask about GPU version and other stuff
   case 0x10:

    gdata&=0xff;

    switch(gdata)
     {
      case 0x02:
       lGPUdataRet=lGPUInfoVals[INFO_TW];              // tw infos
       return;
      case 0x03:
       lGPUdataRet=lGPUInfoVals[INFO_DRAWSTART];       // draw start
       return;
      case 0x04:
       lGPUdataRet=lGPUInfoVals[INFO_DRAWEND];         // draw end
       return;
      case 0x05:
      case 0x06:
       lGPUdataRet=lGPUInfoVals[INFO_DRAWOFF];         // draw offset
       return;
      case 0x07:
       if(dwGPUVersion==2)
            lGPUdataRet=0x01;
       else lGPUdataRet=0x02;                          // gpu type
       return;
      case 0x08:
      case 0x0F:                                       // some bios addr?
       lGPUdataRet=0xBFC03720;
       return;
     }
    return;
   //--------------------------------------------------//
  }
}

////////////////////////////////////////////////////////////////////////
// vram read/write helpers, needed by LEWPY's optimized vram read/write :)
////////////////////////////////////////////////////////////////////////

__inline void FinishedVRAMWrite(void)
{
/*
// NEWX
 if(!PSXDisplay.Interlaced && UseFrameSkip)            // stupid frame skipping
  {
   VRAMWrite.Width +=VRAMWrite.x;
   VRAMWrite.Height+=VRAMWrite.y;
   if(VRAMWrite.x<PSXDisplay.DisplayEnd.x &&
      VRAMWrite.Width >=PSXDisplay.DisplayPosition.x &&
      VRAMWrite.y<PSXDisplay.DisplayEnd.y &&
      VRAMWrite.Height>=PSXDisplay.DisplayPosition.y)
    updateDisplay();
  }
*/

 // Set register to NORMAL operation
 DataWriteMode = DR_NORMAL;
 // Reset transfer values, to prevent mis-transfer of data
 VRAMWrite.x = 0;
 VRAMWrite.y = 0;
 VRAMWrite.Width = 0;
 VRAMWrite.Height = 0;
 VRAMWrite.ColsRemaining = 0;
 VRAMWrite.RowsRemaining = 0;
}

__inline void FinishedVRAMRead(void)
{
 // Set register to NORMAL operation
 DataReadMode = DR_NORMAL;
 // Reset transfer values, to prevent mis-transfer of data
 VRAMRead.x = 0;
 VRAMRead.y = 0;
 VRAMRead.Width = 0;
 VRAMRead.Height = 0;
 VRAMRead.ColsRemaining = 0;
 VRAMRead.RowsRemaining = 0;

 // Indicate GPU is no longer ready for VRAM data in the STATUS REGISTER
 lGPUstatusRet&=~GPUSTATUS_READYFORVRAM;
}

////////////////////////////////////////////////////////////////////////
// core read from vram
////////////////////////////////////////////////////////////////////////

void PEOPS_GPUreadDataMem(unsigned long * pMem, int iSize)
{
 int i;

 if(DataReadMode!=DR_VRAMTRANSFER) return;

 GPUIsBusy;

 // adjust read ptr, if necessary
 while(VRAMRead.ImagePtr>=psxVuw_eom)
  VRAMRead.ImagePtr-=iGPUHeight*1024;
 while(VRAMRead.ImagePtr<psxVuw)
  VRAMRead.ImagePtr+=iGPUHeight*1024;

 for(i=0;i<iSize;i++)
  {
   // do 2 seperate 16bit reads for compatibility (wrap issues)
   if ((VRAMRead.ColsRemaining > 0) && (VRAMRead.RowsRemaining > 0))
    {
     // lower 16 bit
     lGPUdataRet=(unsigned long)GETLE16(VRAMRead.ImagePtr);

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
     lGPUdataRet|=(unsigned long)GETLE16(VRAMRead.ImagePtr)<<16;
     PUTLE32(pMem, lGPUdataRet); pMem++;

     if(VRAMRead.ColsRemaining <= 0)
      {FinishedVRAMRead();goto ENDREAD;}

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
      {FinishedVRAMRead();goto ENDREAD;}
    }
   else {FinishedVRAMRead();goto ENDREAD;}
  }

ENDREAD:
 GPUIsIdle;
}


////////////////////////////////////////////////////////////////////////

unsigned long PEOPS_GPUreadData(void)
{
 unsigned long l;
 PEOPS_GPUreadDataMem(&l,1);
 return lGPUdataRet;
}

////////////////////////////////////////////////////////////////////////
// processes data send to GPU data register
// extra table entries for fixing polyline troubles
////////////////////////////////////////////////////////////////////////

const unsigned char primTableCX[256] =
{
    // 00
    0,0,3,0,0,0,0,0,
    // 08
    0,0,0,0,0,0,0,0,
    // 10
    0,0,0,0,0,0,0,0,
    // 18
    0,0,0,0,0,0,0,0,
    // 20
    4,4,4,4,7,7,7,7,
    // 28
    5,5,5,5,9,9,9,9,
    // 30
    6,6,6,6,9,9,9,9,
    // 38
    8,8,8,8,12,12,12,12,
    // 40
    3,3,3,3,0,0,0,0,
    // 48
//  5,5,5,5,6,6,6,6,    // FLINE
    254,254,254,254,254,254,254,254,
    // 50
    4,4,4,4,0,0,0,0,
    // 58
//  7,7,7,7,9,9,9,9,    // GLINE
    255,255,255,255,255,255,255,255,
    // 60
    3,3,3,3,4,4,4,4,
    // 68
    2,2,2,2,3,3,3,3,    // 3=SPRITE1???
    // 70
    2,2,2,2,3,3,3,3,
    // 78
    2,2,2,2,3,3,3,3,
    // 80
    4,0,0,0,0,0,0,0,
    // 88
    0,0,0,0,0,0,0,0,
    // 90
    0,0,0,0,0,0,0,0,
    // 98
    0,0,0,0,0,0,0,0,
    // a0
    3,0,0,0,0,0,0,0,
    // a8
    0,0,0,0,0,0,0,0,
    // b0
    0,0,0,0,0,0,0,0,
    // b8
    0,0,0,0,0,0,0,0,
    // c0
    3,0,0,0,0,0,0,0,
    // c8
    0,0,0,0,0,0,0,0,
    // d0
    0,0,0,0,0,0,0,0,
    // d8
    0,0,0,0,0,0,0,0,
    // e0
    0,1,1,1,1,1,1,0,
    // e8
    0,0,0,0,0,0,0,0,
    // f0
    0,0,0,0,0,0,0,0,
    // f8
    0,0,0,0,0,0,0,0
};

void PEOPS_GPUwriteDataMem(unsigned long * pMem, int iSize)
{
 unsigned char command;
 unsigned long gdata=0;
 int i=0;

#ifdef PEOPS_SDLOG
 int jj,jjmax;
	DEBUG_print("append",DBG_SDGECKOAPPEND);
	sprintf(txtbuffer,"Calling GPUwriteDataMem(): mode = %d, *pmem = 0x%8p, iSize = %d\r\n",DataWriteMode,GETLE32(pMem),iSize);
	DEBUG_print(txtbuffer,DBG_SDGECKOPRINT);
	DEBUG_print("close",DBG_SDGECKOCLOSE);
#endif //PEOPS_SDLOG

 GPUIsBusy;
 GPUIsNotReadyForCommands;

STARTVRAM:

 if(DataWriteMode==DR_VRAMTRANSFER)
  {
   BOOL bFinished=FALSE;

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
       if(i>=iSize) {goto ENDVRAM;}
       i++;

       gdata=GETLE32(pMem); pMem++;

       PUTLE16(VRAMWrite.ImagePtr, (unsigned short)gdata); VRAMWrite.ImagePtr++;
       if(VRAMWrite.ImagePtr>=psxVuw_eom) VRAMWrite.ImagePtr-=iGPUHeight*1024;
       VRAMWrite.RowsRemaining --;

       if(VRAMWrite.RowsRemaining <= 0)
        {
         VRAMWrite.ColsRemaining--;
         if (VRAMWrite.ColsRemaining <= 0)             // last pixel is odd width
          {
           gdata=(gdata&0xFFFF)|(((unsigned long)GETLE16(VRAMWrite.ImagePtr))<<16);
           FinishedVRAMWrite();
           bDoVSyncUpdate=TRUE;
           goto ENDVRAM;
          }
         VRAMWrite.RowsRemaining = VRAMWrite.Width;
         VRAMWrite.ImagePtr += 1024 - VRAMWrite.Width;
        }

       PUTLE16(VRAMWrite.ImagePtr, (unsigned short)(gdata>>16)); VRAMWrite.ImagePtr++;
       if(VRAMWrite.ImagePtr>=psxVuw_eom) VRAMWrite.ImagePtr-=iGPUHeight*1024;
       VRAMWrite.RowsRemaining --;
      }

     VRAMWrite.RowsRemaining = VRAMWrite.Width;
     VRAMWrite.ColsRemaining--;
     VRAMWrite.ImagePtr += 1024 - VRAMWrite.Width;
     bFinished=TRUE;
    }

   FinishedVRAMWrite();
   if(bFinished) bDoVSyncUpdate=TRUE;
  }

ENDVRAM:

 if(DataWriteMode==DR_NORMAL)
  {
   void (* *primFunc)(unsigned char *);
   if(bSkipNextFrame) primFunc=primTableSkip;
   else               primFunc=primTableJ;

   for(;i<iSize;)
    {
     if(DataWriteMode==DR_VRAMTRANSFER) goto STARTVRAM;

     gdata=GETLE32(pMem); pMem++; i++;

     if(gpuDataC == 0)
      {
       command = (unsigned char)((gdata>>24) & 0xff);

//if(command>=0xb0 && command<0xc0) auxprintf("b0 %x!!!!!!!!!\n",command);

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
#ifdef PEOPS_SDLOG
	DEBUG_print("append",DBG_SDGECKOAPPEND);
	sprintf(txtbuffer,"  primeFunc[%d](",gpuCommand);
	DEBUG_print(txtbuffer,DBG_SDGECKOPRINT);
	jjmax = (gpuDataC>128) ? 6 : gpuDataP;
	for(jj = 0; jj<jjmax; jj++)
	{
		sprintf(txtbuffer," 0x%8lx",gpuDataM[jj]);
		DEBUG_print(txtbuffer,DBG_SDGECKOPRINT);
	}
	sprintf(txtbuffer,")\r\n");
	DEBUG_print(txtbuffer,DBG_SDGECKOPRINT);
	DEBUG_print("close",DBG_SDGECKOCLOSE);
#endif //PEOPS_SDLOG
       gpuDataC=gpuDataP=0;
       primFunc[gpuCommand]((unsigned char *)gpuDataM);

//       if(dwEmuFixes&0x0001 || dwActFixes&0x0400)      // hack for emulating "gpu busy" in some games
//        iFakePrimBusy=4;
      }
    }
  }

 lGPUdataRet=gdata;

 GPUIsReadyForCommands;
 GPUIsIdle;
}

////////////////////////////////////////////////////////////////////////

void PEOPS_GPUwriteData(unsigned long gdata)
{
 PUTLE32(&gdata, gdata);
 PEOPS_GPUwriteDataMem(&gdata,1);
}

////////////////////////////////////////////////////////////////////////
// this functions will be removed soon (or 'soonish')... not really needed, but some emus want them
////////////////////////////////////////////////////////////////////////

void CALLBACK GPUsetMode(unsigned long gdata)
{
// Peops does nothing here...
// DataWriteMode=(gdata&1)?DR_VRAMTRANSFER:DR_NORMAL;
// DataReadMode =(gdata&2)?DR_VRAMTRANSFER:DR_NORMAL;
}

long CALLBACK GPUgetMode(void)
{
 long iT=0;

 if(DataWriteMode==DR_VRAMTRANSFER) iT|=0x1;
 if(DataReadMode ==DR_VRAMTRANSFER) iT|=0x2;
 return iT;
}

////////////////////////////////////////////////////////////////////////
// call config dlg
////////////////////////////////////////////////////////////////////////

long CALLBACK GPUconfigure(void)
{

 SoftDlgProc();

 return 0;
}

////////////////////////////////////////////////////////////////////////
// sets all kind of act fixes
////////////////////////////////////////////////////////////////////////

void SetFixes(void)
 {
  if(dwActFixes&0x02) sDispWidths[4]=384;
  else                sDispWidths[4]=368;
 }

////////////////////////////////////////////////////////////////////////
// process gpu commands
////////////////////////////////////////////////////////////////////////

unsigned long lUsedAddr[3];

__inline BOOL CheckForEndlessLoop(unsigned long laddr)
{
 if(laddr==lUsedAddr[1]) return TRUE;
 if(laddr==lUsedAddr[2]) return TRUE;

 if(laddr<lUsedAddr[0]) lUsedAddr[1]=laddr;
 else                   lUsedAddr[2]=laddr;
 lUsedAddr[0]=laddr;
 return FALSE;
}

long PEOPS_GPUdmaChain(unsigned long * baseAddrL, unsigned long addr)
{
 unsigned char * baseAddrB;
 unsigned int DMACommandCounter = 0;

 #ifdef PEOPS_SDLOG
	DEBUG_print("append",DBG_SDGECKOAPPEND);
	sprintf(txtbuffer,"Calling GPUdmaChain(): *baseAddrL = 0x%8p, addr = 0x%8lx\r\n",baseAddrL, addr);
	DEBUG_print(txtbuffer,DBG_SDGECKOPRINT);
	DEBUG_print("close",DBG_SDGECKOCLOSE);
#endif //PEOPS_SDLOG

 GPUIsBusy;

 lUsedAddr[0]=lUsedAddr[1]=lUsedAddr[2]=0xffffff;

 baseAddrB = (unsigned char*) baseAddrL;

 do
  {
   if(iGPUHeight==512) addr&=0x1FFFFC;
   if(DMACommandCounter++ > 2000000) break;
   if(CheckForEndlessLoop(addr)) break;

   short count = baseAddrB[addr+3];

   unsigned long dmaMem=addr+4;

   if(count>0) PEOPS_GPUwriteDataMem(&baseAddrL[dmaMem>>2],count);

   addr = GETLE32(&baseAddrL[addr>>2])&0xffffff;
  }
 while (addr != 0xffffff);

 GPUIsIdle;

 return 0;
}

////////////////////////////////////////////////////////////////////////
// show about dlg
////////////////////////////////////////////////////////////////////////

void CALLBACK GPUabout(void)                           // ABOUT
{

#ifndef _FPSE
 AboutDlgProc();
#endif
 return;
}

////////////////////////////////////////////////////////////////////////
// We are ever fine ;)
////////////////////////////////////////////////////////////////////////

long CALLBACK GPUtest(void)
{
 // if test fails this function should return negative value for error (unable to continue)
 // and positive value for warning (can continue but output might be crappy)
 return 0;
}

////////////////////////////////////////////////////////////////////////
// Freeze
////////////////////////////////////////////////////////////////////////

typedef struct GPUFREEZETAG
{
 unsigned long ulFreezeVersion;      // should be always 1 for now (set by main emu)
 unsigned long ulStatus;             // current gpu status
 unsigned long ulControl[256];       // latest control register values
 //unsigned char psxVRam[1024*1024*2]; // current VRam image (full 2 MB for ZN)
} GPUFreeze_t;

////////////////////////////////////////////////////////////////////////

long PEOPS_GPUfreeze(unsigned long ulGetFreezeData,GPUFreeze_t * pF)
{
 //----------------------------------------------------//
 if(ulGetFreezeData==2)                                // 2: info, which save slot is selected? (just for display)
  {
   long lSlotNum=*((long *)pF);
   if(lSlotNum<0) return 0;
   if(lSlotNum>8) return 0;
   lSelectedSlot=lSlotNum+1;
   BuildDispMenu(0);
   return 1;
  }
 //----------------------------------------------------//
 if(!pF)                    return 0;                  // some checks
 if(pF->ulFreezeVersion!=1) return 0;

 if(ulGetFreezeData==1)                                // 1: get data (Save State)
  {
   pF->ulStatus=lGPUstatusRet;
   memcpy(pF->ulControl,ulStatusControl,256*sizeof(unsigned long));
   //memcpy(pF->psxVRam,  psxVub,         1024*iGPUHeight*2); //done in Misc.c

   return 1;
  }

 if(ulGetFreezeData!=0) return 0;                      // 0: set data (Load State)

 lGPUstatusRet=pF->ulStatus;
 memcpy(ulStatusControl,pF->ulControl,256*sizeof(unsigned long));
 //memcpy(psxVub,         pF->psxVRam,  1024*iGPUHeight*2); //done in Misc.c

// RESET TEXTURE STORE HERE, IF YOU USE SOMETHING LIKE THAT

 PEOPS_GPUwriteStatus(ulStatusControl[0]);
 PEOPS_GPUwriteStatus(ulStatusControl[1]);
 PEOPS_GPUwriteStatus(ulStatusControl[2]);
 PEOPS_GPUwriteStatus(ulStatusControl[3]);
 PEOPS_GPUwriteStatus(ulStatusControl[8]);                   // try to repair things
 PEOPS_GPUwriteStatus(ulStatusControl[6]);
 PEOPS_GPUwriteStatus(ulStatusControl[7]);
 PEOPS_GPUwriteStatus(ulStatusControl[5]);
 PEOPS_GPUwriteStatus(ulStatusControl[4]);

 return 1;
}

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
// SAVE STATE DISPLAY STUFF
////////////////////////////////////////////////////////////////////////

// font 0-9, 24x20 pixels, 1 byte = 4 dots
// 00 = black
// 01 = white
// 10 = red
// 11 = transparent

unsigned char cFont[10][120]=
{
// 0
{0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x05,0x54,0x00,0x00,
 0x80,0x00,0x14,0x05,0x00,0x00,
 0x80,0x00,0x14,0x05,0x00,0x00,
 0x80,0x00,0x14,0x05,0x00,0x00,
 0x80,0x00,0x14,0x05,0x00,0x00,
 0x80,0x00,0x14,0x05,0x00,0x00,
 0x80,0x00,0x14,0x05,0x00,0x00,
 0x80,0x00,0x14,0x05,0x00,0x00,
 0x80,0x00,0x14,0x05,0x00,0x00,
 0x80,0x00,0x05,0x54,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0xaa,0xaa,0xaa,0xaa,0xaa,0xaa
},
// 1
{0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x50,0x00,0x00,
 0x80,0x00,0x05,0x50,0x00,0x00,
 0x80,0x00,0x00,0x50,0x00,0x00,
 0x80,0x00,0x00,0x50,0x00,0x00,
 0x80,0x00,0x00,0x50,0x00,0x00,
 0x80,0x00,0x00,0x50,0x00,0x00,
 0x80,0x00,0x00,0x50,0x00,0x00,
 0x80,0x00,0x00,0x50,0x00,0x00,
 0x80,0x00,0x00,0x50,0x00,0x00,
 0x80,0x00,0x05,0x55,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0xaa,0xaa,0xaa,0xaa,0xaa,0xaa
},
// 2
{0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x05,0x54,0x00,0x00,
 0x80,0x00,0x14,0x05,0x00,0x00,
 0x80,0x00,0x00,0x05,0x00,0x00,
 0x80,0x00,0x00,0x05,0x00,0x00,
 0x80,0x00,0x00,0x14,0x00,0x00,
 0x80,0x00,0x00,0x50,0x00,0x00,
 0x80,0x00,0x01,0x40,0x00,0x00,
 0x80,0x00,0x05,0x00,0x00,0x00,
 0x80,0x00,0x14,0x00,0x00,0x00,
 0x80,0x00,0x15,0x55,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0xaa,0xaa,0xaa,0xaa,0xaa,0xaa
},
// 3
{0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x05,0x54,0x00,0x00,
 0x80,0x00,0x14,0x05,0x00,0x00,
 0x80,0x00,0x00,0x05,0x00,0x00,
 0x80,0x00,0x00,0x05,0x00,0x00,
 0x80,0x00,0x01,0x54,0x00,0x00,
 0x80,0x00,0x00,0x05,0x00,0x00,
 0x80,0x00,0x00,0x05,0x00,0x00,
 0x80,0x00,0x00,0x05,0x00,0x00,
 0x80,0x00,0x14,0x05,0x00,0x00,
 0x80,0x00,0x05,0x54,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0xaa,0xaa,0xaa,0xaa,0xaa,0xaa
},
// 4
{0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x14,0x00,0x00,
 0x80,0x00,0x00,0x54,0x00,0x00,
 0x80,0x00,0x01,0x54,0x00,0x00,
 0x80,0x00,0x01,0x54,0x00,0x00,
 0x80,0x00,0x05,0x14,0x00,0x00,
 0x80,0x00,0x14,0x14,0x00,0x00,
 0x80,0x00,0x15,0x55,0x00,0x00,
 0x80,0x00,0x00,0x14,0x00,0x00,
 0x80,0x00,0x00,0x14,0x00,0x00,
 0x80,0x00,0x00,0x55,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0xaa,0xaa,0xaa,0xaa,0xaa,0xaa
},
// 5
{0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x15,0x55,0x00,0x00,
 0x80,0x00,0x14,0x00,0x00,0x00,
 0x80,0x00,0x14,0x00,0x00,0x00,
 0x80,0x00,0x14,0x00,0x00,0x00,
 0x80,0x00,0x15,0x54,0x00,0x00,
 0x80,0x00,0x00,0x05,0x00,0x00,
 0x80,0x00,0x00,0x05,0x00,0x00,
 0x80,0x00,0x00,0x05,0x00,0x00,
 0x80,0x00,0x14,0x05,0x00,0x00,
 0x80,0x00,0x05,0x54,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0xaa,0xaa,0xaa,0xaa,0xaa,0xaa
},
// 6
{0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x01,0x54,0x00,0x00,
 0x80,0x00,0x05,0x00,0x00,0x00,
 0x80,0x00,0x14,0x00,0x00,0x00,
 0x80,0x00,0x14,0x00,0x00,0x00,
 0x80,0x00,0x15,0x54,0x00,0x00,
 0x80,0x00,0x15,0x05,0x00,0x00,
 0x80,0x00,0x14,0x05,0x00,0x00,
 0x80,0x00,0x14,0x05,0x00,0x00,
 0x80,0x00,0x14,0x05,0x00,0x00,
 0x80,0x00,0x05,0x54,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0xaa,0xaa,0xaa,0xaa,0xaa,0xaa
},
// 7
{0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x15,0x55,0x00,0x00,
 0x80,0x00,0x14,0x05,0x00,0x00,
 0x80,0x00,0x00,0x14,0x00,0x00,
 0x80,0x00,0x00,0x14,0x00,0x00,
 0x80,0x00,0x00,0x50,0x00,0x00,
 0x80,0x00,0x00,0x50,0x00,0x00,
 0x80,0x00,0x01,0x40,0x00,0x00,
 0x80,0x00,0x01,0x40,0x00,0x00,
 0x80,0x00,0x05,0x00,0x00,0x00,
 0x80,0x00,0x05,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0xaa,0xaa,0xaa,0xaa,0xaa,0xaa
},
// 8
{0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x05,0x54,0x00,0x00,
 0x80,0x00,0x14,0x05,0x00,0x00,
 0x80,0x00,0x14,0x05,0x00,0x00,
 0x80,0x00,0x14,0x05,0x00,0x00,
 0x80,0x00,0x05,0x54,0x00,0x00,
 0x80,0x00,0x14,0x05,0x00,0x00,
 0x80,0x00,0x14,0x05,0x00,0x00,
 0x80,0x00,0x14,0x05,0x00,0x00,
 0x80,0x00,0x14,0x05,0x00,0x00,
 0x80,0x00,0x05,0x54,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0xaa,0xaa,0xaa,0xaa,0xaa,0xaa
},
// 9
{0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x05,0x54,0x00,0x00,
 0x80,0x00,0x14,0x05,0x00,0x00,
 0x80,0x00,0x14,0x05,0x00,0x00,
 0x80,0x00,0x14,0x05,0x00,0x00,
 0x80,0x00,0x14,0x15,0x00,0x00,
 0x80,0x00,0x05,0x55,0x00,0x00,
 0x80,0x00,0x00,0x05,0x00,0x00,
 0x80,0x00,0x00,0x05,0x00,0x00,
 0x80,0x00,0x00,0x14,0x00,0x00,
 0x80,0x00,0x05,0x50,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0x80,0x00,0x00,0x00,0x00,0x00,
 0xaa,0xaa,0xaa,0xaa,0xaa,0xaa
}
};

////////////////////////////////////////////////////////////////////////

void PaintPicDot(unsigned char * p,unsigned char c)
{

 if(c==0) {*p++=0x00;*p++=0x00;*p=0x00;return;}        // black
 if(c==1) {*p++=0xff;*p++=0xff;*p=0xff;return;}        // white
 if(c==2) {*p++=0x00;*p++=0x00;*p=0xff;return;}        // red
                                                       // transparent
}

////////////////////////////////////////////////////////////////////////
// the main emu allocs 128x96x3 bytes, and passes a ptr
// to it in pMem... the plugin has to fill it with
// 8-8-8 bit BGR screen data (Win 24 bit BMP format
// without header).
// Beware: the func can be called at any time,
// so you have to use the frontbuffer to get a fully
// rendered picture

extern char * Xpixels;

void GPUgetScreenPic(unsigned char * pMem)
{
	//Unsupported
}

////////////////////////////////////////////////////////////////////////
// func will be called with 128x96x3 BGR data.
// the plugin has to store the data and display
// it in the upper right corner.
// If the func is called with a NULL ptr, you can
// release your picture data and stop displaying
// the screen pic

void CALLBACK GPUshowScreenPic(unsigned char * pMem)
{
 DestroyPic();                                         // destroy old pic data
 if(pMem==0) return;                                   // done
 CreatePic(pMem);                                      // create new pic... don't free pMem or something like that... just read from it
}

////////////////////////////////////////////////////////////////////////

void CALLBACK GPUsetfix(unsigned long dwFixBits)
{
 dwEmuFixes=dwFixBits;
}

////////////////////////////////////////////////////////////////////////

void CALLBACK GPUsetframelimit(unsigned long option)
{
 bInitCap = TRUE;

if (frameLimit == FRAMELIMIT_AUTO)
 {
	UseFrameLimit=1;
	iFrameLimit=2;
	SetAutoFrameCap();
 }
 else
 {
	UseFrameLimit=0;
	iFrameLimit=0;
 }
 UseFrameSkip = frameSkip;
 BuildDispMenu(0);
}
