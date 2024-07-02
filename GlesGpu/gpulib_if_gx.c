/***************************************************************************
    begin                : Sun Mar 08 2009
    copyright            : (C) 1999-2009 by Pete Bernert
    email                : BlackDove@addcom.de

    PCSX rearmed rework (C) notaz, 2012
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
#include <gccore.h>
#include "../Gamecube/MEM2.h"
#include "../Gamecube/DEBUG.h"
#include "../SoftGPU/oldGpuFps.h"

#define TEXTURE_BLOCK_SIZE   (256 * 256 * 4)
#define TEXTURE_MOVIE_IDX    48
#define TEXTURE_FRAME_IDX    49
#define TEXTURE_MAX_COUNT    50


#define _IN_GPU_LIB

#include "gpuExternals.h"
#include "gpuStdafx.h"
#include "gpuDraw.c"
#include "gpuTexture.c"
#include "gpuPrim.c"


//int           gInterlaceLine=1;
static const short dispWidths[8] = {256,320,512,640,368,384,512,640};
//short g_m1,g_m2,g_m3;
//short DrawSemiTrans;

//short   ly0,lx0,ly1,lx1,ly2,lx2,ly3,lx3;        // global psx vertex coords
//int            GlobalTextAddrX,GlobalTextAddrY,GlobalTextTP;
//int            GlobalTextREST,GlobalTextABR,GlobalTextPAGE;

unsigned int  dwGPUVersion;
int           iGPUHeight=512;
int           iGPUHeightMask=511;
//int           GlobalTextIL;

//unsigned char  *psxVub;
//unsigned short *psxVuw;

//float           gl_z=0.0f;
//BOOL            bNeedInterlaceUpdate;
//BOOL            bNeedRGB24Update;
//BOOL            bChangeWinMode;
//int             lGPUstatusRet;
//unsigned int    ulGPUInfoVals[16];
//VRAMLoad_t      VRAMWrite;
//VRAMLoad_t      VRAMRead;
//int             iDataWriteMode;
//int             iDataReadMode;

//int             lClearOnSwap;
//int             lClearOnSwapColor;
//BOOL            bSkipNextFrame;

//PSXDisplay_t    PSXDisplay;
//PSXDisplay_t    PreviousPSXDisplay;
//TWin_t          TWin;
//BOOL            bDisplayNotSet;
//BOOL            bNeedWriteUpload;
//int             iLastRGB24;

// don't do GL vram read
void CheckVRamRead(int x, int y, int dx, int dy, bool bFront)
{
}

//void CheckVRamReadEx(int x, int y, int dx, int dy)
//{
//}
//
//void SetFixes(void)
//{
//}

static void PaintBlackBorders(void)
{
 short s;
 glDisable(GL_SCISSOR_TEST); glError();
 if(bTexEnabled) {glDisable(GL_TEXTURE_2D);bTexEnabled=FALSE;} glError();
 if(bOldSmoothShaded) {glShadeModel(GL_FLAT);bOldSmoothShaded=FALSE;} glError();
 if(bBlendEnable)     {glDisable(GL_BLEND);bBlendEnable=FALSE;} glError();
 glDisable(GL_ALPHA_TEST); glError();

 glEnable(GL_ALPHA_TEST); glError();
 glEnable(GL_SCISSOR_TEST); glError();
}

//static void fps_update(void);

//void updateDisplayGl(void)
//{
// bFakeFrontBuffer=FALSE;
// bRenderFrontBuffer=FALSE;
//
// if(PSXDisplay.RGB24)// && !bNeedUploadAfter)         // (mdec) upload wanted?
// {
//  PrepareFullScreenUpload(-1);
//  UploadScreen(PSXDisplay.Interlaced);                // -> upload whole screen from psx vram
//  bNeedUploadTest=FALSE;
//  bNeedInterlaceUpdate=FALSE;
//  bNeedUploadAfter=FALSE;
//  bNeedRGB24Update=FALSE;
// }
// else
// if(bNeedInterlaceUpdate)                             // smaller upload?
// {
//  bNeedInterlaceUpdate=FALSE;
//  xrUploadArea=xrUploadAreaIL;                        // -> upload this rect
//  UploadScreen(TRUE);
// }
//
// if(dwActFixes&512) bCheckFF9G4(NULL);                // special game fix for FF9
//
// if(PSXDisplay.Disabled)                              // display disabled?
// {
//  // moved here
//  glDisable(GL_SCISSOR_TEST); glError();
//  glClearColor(0,0,0,128); glError();                 // -> clear whole backbuffer
//  glClear(uiBufferBits); glError();
//  glEnable(GL_SCISSOR_TEST); glError();
//  gl_z=0.0f;
//  bDisplayNotSet = TRUE;
// }
//
// if(iDrawnSomething)
// {
//  //fps_update();
//  //eglSwapBuffers(display, surface);
//  gc_vout_render();
//  iDrawnSomething=0;
// }
//
// if(lClearOnSwap)                                     // clear buffer after swap?
// {
//  GLclampf g,b,r;
//
//  if(bDisplayNotSet)                                  // -> set new vals
//   SetOGLDisplaySettings(1);
//
//  g=((GLclampf)GREEN(lClearOnSwapColor))/255.0f;      // -> get col
//  b=((GLclampf)BLUE(lClearOnSwapColor))/255.0f;
//  r=((GLclampf)RED(lClearOnSwapColor))/255.0f;
//  glDisable(GL_SCISSOR_TEST); glError();
//  glClearColor(r,g,b,128); glError();                 // -> clear
//  glClear(uiBufferBits); glError();
//  glEnable(GL_SCISSOR_TEST); glError();
//  lClearOnSwap=0;                                     // -> done
// }
// else
// {
//  if(iZBufferDepth)                                   // clear zbuffer as well (if activated)
//   {
//    glDisable(GL_SCISSOR_TEST); glError();
//    glClear(GL_DEPTH_BUFFER_BIT); glError();
//    glEnable(GL_SCISSOR_TEST); glError();
//   }
// }
// gl_z=0.0f;
//
// // additional uploads immediatly after swapping
// if(bNeedUploadAfter)                                 // upload wanted?
// {
//  bNeedUploadAfter=FALSE;
//  bNeedUploadTest=FALSE;
//  UploadScreen(-1);                                   // -> upload
// }
//
// if(bNeedUploadTest)
// {
//  bNeedUploadTest=FALSE;
//  if(PSXDisplay.InterlacedTest &&
//     //iOffscreenDrawing>2 &&
//     PreviousPSXDisplay.DisplayPosition.x==PSXDisplay.DisplayPosition.x &&
//     PreviousPSXDisplay.DisplayEnd.x==PSXDisplay.DisplayEnd.x &&
//     PreviousPSXDisplay.DisplayPosition.y==PSXDisplay.DisplayPosition.y &&
//     PreviousPSXDisplay.DisplayEnd.y==PSXDisplay.DisplayEnd.y)
//   {
//    PrepareFullScreenUpload(TRUE);
//    UploadScreen(TRUE);
//   }
// }
//}

//void updateFrontDisplayGl(void)
//{
// if(PreviousPSXDisplay.Range.x0||
//    PreviousPSXDisplay.Range.y0)
//  PaintBlackBorders();
//
// bFakeFrontBuffer=FALSE;
// bRenderFrontBuffer=FALSE;
//
// if(iDrawnSomething)                                  // linux:
//  //eglSwapBuffers(display, surface); TODO
//   gc_vout_render();
//}

//static void ChangeDispOffsetsX(void)                  // CENTER X
//{
//int lx,l;short sO;
//
//if(!PSXDisplay.Range.x1) return;                      // some range given?
//
//l=PSXDisplay.DisplayMode.x;
//
//l*=(int)PSXDisplay.Range.x1;                         // some funky calculation
//l/=2560;lx=l;l&=0xfffffff8;
//
//if(l==PreviousPSXDisplay.Range.x1) return;            // some change?
//
//sO=PreviousPSXDisplay.Range.x0;                       // store old
//
//if(lx>=PSXDisplay.DisplayMode.x)                      // range bigger?
// {
//  PreviousPSXDisplay.Range.x1=                        // -> take display width
//   PSXDisplay.DisplayMode.x;
//  PreviousPSXDisplay.Range.x0=0;                      // -> start pos is 0
// }
//else                                                  // range smaller? center it
// {
//  PreviousPSXDisplay.Range.x1=l;                      // -> store width (8 pixel aligned)
//   PreviousPSXDisplay.Range.x0=                       // -> calc start pos
//   (PSXDisplay.Range.x0-500)/8;
//  if(PreviousPSXDisplay.Range.x0<0)                   // -> we don't support neg. values yet
//   PreviousPSXDisplay.Range.x0=0;
//
//  if((PreviousPSXDisplay.Range.x0+lx)>                // -> uhuu... that's too much
//     PSXDisplay.DisplayMode.x)
//   {
//    PreviousPSXDisplay.Range.x0=                      // -> adjust start
//     PSXDisplay.DisplayMode.x-lx;
//    PreviousPSXDisplay.Range.x1+=lx-l;                // -> adjust width
//   }
// }
//
//if(sO!=PreviousPSXDisplay.Range.x0)                   // something changed?
// {
//  bDisplayNotSet=TRUE;                                // -> recalc display stuff
// }
//}

////////////////////////////////////////////////////////////////////////

//static void ChangeDispOffsetsY(void)                  // CENTER Y
//{
//int iT;short sO;                                      // store previous y size
//
//if(PSXDisplay.PAL) iT=48; else iT=28;                 // different offsets on PAL/NTSC
//
//if(PSXDisplay.Range.y0>=iT)                           // crossed the security line? :)
// {
//  PreviousPSXDisplay.Range.y1=                        // -> store width
//   PSXDisplay.DisplayModeNew.y;
//
//  sO=(PSXDisplay.Range.y0-iT-4)*PSXDisplay.Double;    // -> calc offset
//  if(sO<0) sO=0;
//
//  PSXDisplay.DisplayModeNew.y+=sO;                    // -> add offset to y size, too
// }
//else sO=0;                                            // else no offset
//
//if(sO!=PreviousPSXDisplay.Range.y0)                   // something changed?
// {
//  PreviousPSXDisplay.Range.y0=sO;
//  bDisplayNotSet=TRUE;                                // -> recalc display stuff
// }
//}

//static void updateDisplayIfChanged(void)
//{
//BOOL bUp;
//
//if ((PSXDisplay.DisplayMode.y == PSXDisplay.DisplayModeNew.y) &&
//    (PSXDisplay.DisplayMode.x == PSXDisplay.DisplayModeNew.x))
// {
//  if((PSXDisplay.RGB24      == PSXDisplay.RGB24New) &&
//     (PSXDisplay.Interlaced == PSXDisplay.InterlacedNew))
//     return;                                          // nothing has changed? fine, no swap buffer needed
// }
//else                                                  // some res change?
// {
//  glLoadIdentity(); glError();
//  glOrtho(0,PSXDisplay.DisplayModeNew.x,              // -> new psx resolution
//            PSXDisplay.DisplayModeNew.y, 0, -1, 1); glError();
//  if(bKeepRatio) SetAspectRatio();
// }
//
//bDisplayNotSet = TRUE;                                // re-calc offsets/display area
//
//bUp=FALSE;
//if(PSXDisplay.RGB24!=PSXDisplay.RGB24New)             // clean up textures, if rgb mode change (usually mdec on/off)
// {
//  PreviousPSXDisplay.RGB24=0;                         // no full 24 frame uploaded yet
//  ResetTextureArea(FALSE);
//  bUp=TRUE;
// }
//
//PSXDisplay.RGB24         = PSXDisplay.RGB24New;       // get new infos
//PSXDisplay.DisplayMode.y = PSXDisplay.DisplayModeNew.y;
//PSXDisplay.DisplayMode.x = PSXDisplay.DisplayModeNew.x;
//PSXDisplay.Interlaced    = PSXDisplay.InterlacedNew;
//
//PSXDisplay.DisplayEnd.x=                              // calc new ends
// PSXDisplay.DisplayPosition.x+ PSXDisplay.DisplayMode.x;
//PSXDisplay.DisplayEnd.y=
// PSXDisplay.DisplayPosition.y+ PSXDisplay.DisplayMode.y+PreviousPSXDisplay.DisplayModeNew.y;
//PreviousPSXDisplay.DisplayEnd.x=
// PreviousPSXDisplay.DisplayPosition.x+ PSXDisplay.DisplayMode.x;
//PreviousPSXDisplay.DisplayEnd.y=
// PreviousPSXDisplay.DisplayPosition.y+ PSXDisplay.DisplayMode.y+PreviousPSXDisplay.DisplayModeNew.y;
//
//ChangeDispOffsetsX();
//if(bUp) updateDisplayGl();                              // yeah, real update (swap buffer)
//}

#define GPUwriteStatus_ext GPUwriteStatus_ext // for gpulib to see this
void GPUwriteStatus_ext(unsigned int gdata)
{
    #ifdef DISP_DEBUG
    //sprintf(txtbuffer, "GPUwriteStatus_ext 0\r\n");
    //DEBUG_print(txtbuffer, DBG_CORE2);
    #endif // DISP_DEBUG
switch((gdata>>24)&0xff)
 {
  case 0x00:
   PSXDisplay.Disabled=1;
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

  case 0x03:
   PreviousPSXDisplay.Disabled = PSXDisplay.Disabled;
   PSXDisplay.Disabled = (gdata & 1);

   if (iOffscreenDrawing==4 &&
        PreviousPSXDisplay.Disabled &&
       !(PSXDisplay.Disabled))
    {

     if(!PSXDisplay.RGB24)
      {
       PrepareFullScreenUpload(TRUE);
       UploadScreen(TRUE);
       updateDisplayGl();
      }
    }
   return;

  case 0x05:
   {
    short sx=(short)(gdata & 0x3ff);
    short sy;

    sy = (short)((gdata>>10)&0x3ff);             // really: 0x1ff, but we adjust it later
    if (sy & 0x200)
     {
      sy|=0xfc00;
      PreviousPSXDisplay.DisplayModeNew.y=sy/PSXDisplay.Double;
      sy=0;
     }
    else PreviousPSXDisplay.DisplayModeNew.y=0;

    if(sx>1000) sx=0;

    if(dwActFixes&8)
     {
      if((!PSXDisplay.Interlaced) &&
         PreviousPSXDisplay.DisplayPosition.x == sx  &&
         PreviousPSXDisplay.DisplayPosition.y == sy)
       return;

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

    bDisplayNotSet = TRUE;

    if (!(PSXDisplay.Interlaced))
     {
      updateDisplayGl();
     }
    else
    if(PSXDisplay.InterlacedTest &&
       ((PreviousPSXDisplay.DisplayPosition.x != PSXDisplay.DisplayPosition.x)||
        (PreviousPSXDisplay.DisplayPosition.y != PSXDisplay.DisplayPosition.y)))
     PSXDisplay.InterlacedTest--;
    return;
   }

  case 0x06:
   PSXDisplay.Range.x0=gdata & 0x7ff;      //0x3ff;
   PSXDisplay.Range.x1=(gdata>>12) & 0xfff;//0x7ff;

   PSXDisplay.Range.x1-=PSXDisplay.Range.x0;

   ChangeDispOffsetsX();
   return;

  case 0x07:
   PreviousPSXDisplay.Height = PSXDisplay.Height;

   PSXDisplay.Range.y0=gdata & 0x3ff;
   PSXDisplay.Range.y1=(gdata>>10) & 0x3ff;

   PSXDisplay.Height = PSXDisplay.Range.y1 -
                       PSXDisplay.Range.y0 +
                       PreviousPSXDisplay.DisplayModeNew.y;

   if (PreviousPSXDisplay.Height != PSXDisplay.Height)
    {
     PSXDisplay.DisplayModeNew.y=PSXDisplay.Height*PSXDisplay.Double;
     ChangeDispOffsetsY();
     updateDisplayIfChangedGl();
    }
   return;

  case 0x08:
   PSXDisplay.DisplayModeNew.x = dispWidths[(gdata & 0x03) | ((gdata & 0x40) >> 4)];

   if (gdata&0x04) PSXDisplay.Double=2;
   else            PSXDisplay.Double=1;
   PSXDisplay.DisplayModeNew.y = PSXDisplay.Height*PSXDisplay.Double;

   ChangeDispOffsetsY();

   PSXDisplay.PAL           = (gdata & 0x08)?TRUE:FALSE; // if 1 - PAL mode, else NTSC
   PSXDisplay.RGB24New      = (gdata & 0x10)?TRUE:FALSE; // if 1 - TrueColor
   PSXDisplay.InterlacedNew = (gdata & 0x20)?TRUE:FALSE; // if 1 - Interlace

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
    }
   else
    {
     PSXDisplay.InterlacedTest=0;
    }
   updateDisplayIfChangedGl();
   return;
 }
}

/////////////////////////////////////////////////////////////////////////////

#include <stdint.h>

#include "../gpulib/gpu.h"
#include "../gpulib/plugin_lib.h"

//static int is_opened;
//
//static void set_vram(void *vram)
//{
// psxVub=vram;
// psxVuw=(unsigned short *)psxVub;
//}
//
//int renderer_init(void)
//{
// set_vram(gpu.vram);
//
// PSXDisplay.RGB24        = FALSE;                      // init some stuff
// PSXDisplay.Interlaced   = FALSE;
// PSXDisplay.DrawOffset.x = 0;
// PSXDisplay.DrawOffset.y = 0;
// PSXDisplay.DisplayMode.x= 320;
// PSXDisplay.DisplayMode.y= 240;
// PSXDisplay.Disabled     = FALSE;
// PSXDisplay.Range.x0=0;
// PSXDisplay.Range.x1=0;
// PSXDisplay.Double = 1;
//
// lGPUstatusRet = 0x14802000;
//
// return 0;
//}
//
//void renderer_notify_res_change(uint32_t gdata)
//{
//    if (gdata == 0xffffffff)
//    {
//        return;
//    }
//    PSXDisplay.PAL           = (gdata & 0x08)?TRUE:FALSE; // if 1 - PAL mode, else NTSC
//    PSXDisplay.RGB24      = (gdata & 0x10)?TRUE:FALSE; // if 1 - TrueColor
//    PSXDisplay.Interlaced = ((gdata & 0x24) ^ 0x24)?FALSE:TRUE; // if 0 - Interlace
//}

//#include "../gpulib/gpu_timing.h"
//extern const unsigned char cmd_lengths[256];
//
//// XXX: mostly dupe code from soft peops
//int do_cmd_list(uint32_t *list, int list_len,
// int *cycles_sum_out, int *cycles_last, int *last_cmd)
//{
//  int cpu_cycles_sum = 0, cpu_cycles = *cycles_last;
//  unsigned int cmd = 0, len;
//  uint32_t *list_start = list;
//  uint32_t *list_end = list + list_len;
//
//  for (; list < list_end; list += 1 + len)
//  {
//    short *slist = (void *)list;
//    cmd = GETLE32(list) >> 24;
//    len = cmd_lengths[cmd];
//    if (list + 1 + len > list_end) {
//      cmd = -1;
//      break;
//    }
//
//#ifndef TEST
//    if (0x80 <= cmd && cmd < 0xe0)
//      break; // image i/o, forward to upper layer
//    else if ((cmd & 0xf8) == 0xe0)
//      gpu.ex_regs[cmd & 7] = GETLE32(list);
//#endif
//
//    primTableJ[cmd]((void *)list);
//
//    switch(cmd)
//    {
//      case 0x48 ... 0x4F:
//      {
//        u32 num_vertexes = 2;
//        u32 *list_position = &(list[3]);
//
//        while(1)
//        {
//          gput_sum(cpu_cycles_sum, cpu_cycles, gput_line(0));
//
//          if(list_position >= list_end) {
//            cmd = -1;
//            goto breakloop;
//          }
//
//          if((*list_position & SWAP32_C(0xf000f000)) == SWAP32_C(0x50005000))
//            break;
//
//          list_position++;
//          num_vertexes++;
//        }
//
//        len += (num_vertexes - 2);
//        break;
//      }
//
//      case 0x58 ... 0x5F:
//      {
//        u32 num_vertexes = 2;
//        u32 *list_position = &(list[4]);
//
//        while(1)
//        {
//          gput_sum(cpu_cycles_sum, cpu_cycles, gput_line(0));
//
//          if(list_position >= list_end) {
//            cmd = -1;
//            goto breakloop;
//          }
//
//          if((*list_position & SWAP32_C(0xf000f000)) == SWAP32_C(0x50005000))
//            break;
//
//          list_position += 2;
//          num_vertexes++;
//        }
//
//        len += (num_vertexes - 2) * 2;
//        break;
//      }
//
//#ifdef TEST
//      case 0xA0:          //  sys -> vid
//      {
//        u32 load_width = LE2HOST16(slist[4]);
//        u32 load_height = LE2HOST16(slist[5]);
//        u32 load_size = load_width * load_height;
//
//        len += load_size / 2;
//        break;
//      }
//#endif
//
//      // timing
//      case 0x02:
//        gput_sum(cpu_cycles_sum, cpu_cycles,
//            gput_fill(LE2HOST16(slist[4]) & 0x3ff, LE2HOST16(slist[5]) & 0x1ff));
//        break;
//      case 0x20 ... 0x23: gput_sum(cpu_cycles_sum, cpu_cycles, gput_poly_base());    break;
//      case 0x24 ... 0x27: gput_sum(cpu_cycles_sum, cpu_cycles, gput_poly_base_t());  break;
//      case 0x28 ... 0x2B: gput_sum(cpu_cycles_sum, cpu_cycles, gput_quad_base());    break;
//      case 0x2C ... 0x2F: gput_sum(cpu_cycles_sum, cpu_cycles, gput_quad_base_t());  break;
//      case 0x30 ... 0x33: gput_sum(cpu_cycles_sum, cpu_cycles, gput_poly_base_g());  break;
//      case 0x34 ... 0x37: gput_sum(cpu_cycles_sum, cpu_cycles, gput_poly_base_gt()); break;
//      case 0x38 ... 0x3B: gput_sum(cpu_cycles_sum, cpu_cycles, gput_quad_base_g());  break;
//      case 0x3C ... 0x3F: gput_sum(cpu_cycles_sum, cpu_cycles, gput_quad_base_gt()); break;
//      case 0x40 ... 0x47: gput_sum(cpu_cycles_sum, cpu_cycles, gput_line(0));        break;
//      case 0x50 ... 0x57: gput_sum(cpu_cycles_sum, cpu_cycles, gput_line(0));        break;
//      case 0x60 ... 0x63:
//        gput_sum(cpu_cycles_sum, cpu_cycles,
//            gput_sprite(LE2HOST16(slist[4]) & 0x3ff, LE2HOST16(slist[5]) & 0x1ff));
//        break;
//      case 0x64 ... 0x67:
//        gput_sum(cpu_cycles_sum, cpu_cycles,
//            gput_sprite(LE2HOST16(slist[6]) & 0x3ff, LE2HOST16(slist[7]) & 0x1ff));
//        break;
//      case 0x68 ... 0x6B: gput_sum(cpu_cycles_sum, cpu_cycles, gput_sprite(1, 1));   break;
//      case 0x70 ... 0x73:
//      case 0x74 ... 0x77: gput_sum(cpu_cycles_sum, cpu_cycles, gput_sprite(8, 8));   break;
//      case 0x78 ... 0x7B:
//      case 0x7C ... 0x7F: gput_sum(cpu_cycles_sum, cpu_cycles, gput_sprite(16, 16)); break;
//    }
//  }
//
//breakloop:
//  gpu.ex_regs[1] &= ~0x1ff;
//  gpu.ex_regs[1] |= lGPUstatusRet & 0x1ff;
//
//  *cycles_sum_out += cpu_cycles_sum;
//  *cycles_last = cpu_cycles;
//  *last_cmd = cmd;
//  return list - list_start;
//}
//
//void renderer_sync_ecmds(uint32_t *ecmds_)
//{
//#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
//  // the funcs below expect LE
//  uint32_t i, ecmds[8];
//  for (i = 1; i <= 6; i++)
//    ecmds[i] = GETLE32(&ecmds_[i]);
//#else
//  uint32_t *ecmds = ecmds_;
//#endif
//  cmdTexturePage((unsigned char *)&ecmds[1]);
//  cmdTextureWindow((unsigned char *)&ecmds[2]);
//  cmdDrawAreaStart((unsigned char *)&ecmds[3]);
//  cmdDrawAreaEnd((unsigned char *)&ecmds[4]);
//  cmdDrawOffset((unsigned char *)&ecmds[5]);
//  cmdSTP((unsigned char *)&ecmds[6]);
//}
//
//void renderer_update_caches(int x, int y, int w, int h, int state_changed)
//{
// VRAMWrite.x = x;
// VRAMWrite.y = y;
// VRAMWrite.Width = w;
// VRAMWrite.Height = h;
// if(is_opened)
//  CheckWriteUpdate();
//}
//
//static struct rearmed_cbs *cbs;

//void plugin_call_rearmed_cbs(const struct rearmed_cbs *_cbs)
//{
//	extern void *hGPUDriver;
//	void (*rearmed_set_cbs)(const struct rearmed_cbs *_cbs);
//
//	rearmed_set_cbs = SysLoadSym(hGPUDriver, "GPUrearmedCallbacks");
//	if (rearmed_set_cbs != NULL)
//	{
//		rearmed_set_cbs(_cbs);
//	}
//}

extern void CALLBACK GPUsetframelimit(unsigned long option);
long GL_GPUopen()
{
 #ifdef DISP_DEBUG
 //writeLogFile("GL_GPUopen 0\r\n");
 #endif // DISP_DEBUG
 int ret;

 InitFPS();

 GPUsetframelimit(0);

 iResX = 640;
 iResY = 480;
 rRatioRect.left   = rRatioRect.top=0;
 rRatioRect.right  = iResX;
 rRatioRect.bottom = iResY;

 bDisplayNotSet = TRUE;
 bSetClip = TRUE;
 CSTEXTURE = CSVERTEX = CSCOLOR = 0;

 #ifdef DISP_DEBUG
 //writeLogFile("GL_GPUopen 0.1\r\n");
 #endif // DISP_DEBUG
 InitializeTextureStore();                             // init texture mem

 #ifdef DISP_DEBUG
 //writeLogFile("GL_GPUopen 1\r\n");
 #endif // DISP_DEBUG
 ret = GLinitialize(NULL, NULL);
 #ifdef DISP_DEBUG
 //writeLogFile("GL_GPUopen End\r\n");
 #endif // DISP_DEBUG
 //MakeDisplayLists();

 gx_vout_open();

// is_opened = 1;
 return ret;
}

long GL_GPUclose(void)
{
    #ifdef DISP_DEBUG
 //writeLogFile("GL_GPUclose 0\r\n");
 #endif // DISP_DEBUG
// is_opened = 0;

 //KillDisplayLists();
 GLcleanup();                                          // close OGL
 return 0;
}

/* acting as both renderer and vout handler here .. */
//void renderer_set_config(const struct rearmed_cbs *cbs_)
//{
//    #ifdef DISP_DEBUG
// writeLogFile("renderer_set_config 0\r\n");
// #endif // DISP_DEBUG
// cbs = (void *)cbs_; // ugh..
//
// iOffscreenDrawing = 0;
// iZBufferDepth = 0;
// iFrameReadType = 0;
// bKeepRatio = TRUE;
//
// dwActFixes = cbs->gpu_peopsgl.dwActFixes;
// bDrawDither = cbs->gpu_peopsgl.bDrawDither;
// iFilterType = cbs->gpu_peopsgl.iFilterType;
// iFrameTexType = cbs->gpu_peopsgl.iFrameTexType;
// iUseMask = cbs->gpu_peopsgl.iUseMask;
// bOpaquePass = cbs->gpu_peopsgl.bOpaquePass;
// bAdvancedBlend = cbs->gpu_peopsgl.bAdvancedBlend;
// bUseFastMdec = cbs->gpu_peopsgl.bUseFastMdec;
// iTexGarbageCollection = cbs->gpu_peopsgl.iTexGarbageCollection;
// iVRamSize = cbs->gpu_peopsgl.iVRamSize;
//
// if (cbs->pl_set_gpu_caps)
//  cbs->pl_set_gpu_caps(GPU_CAP_OWNS_DISPLAY);
//
// //if (is_opened && cbs->gles_display != NULL && cbs->gles_surface != NULL) {
//  // HACK..
// // GL_GPUclose();
// // GL_GPUopen(NULL, NULL, NULL);
// //}
//
// set_vram(gpu.vram);
// #ifdef DISP_DEBUG
// writeLogFile("renderer_set_config 1\r\n");
// #endif // DISP_DEBUG
//}

void SetAspectRatio(void)
{
 //if (cbs->pl_get_layer_pos)
 // cbs->pl_get_layer_pos(&rRatioRect.left, &rRatioRect.top, &rRatioRect.right, &rRatioRect.bottom);

 glScissor(rRatioRect.left,
           iResY-(rRatioRect.top+rRatioRect.bottom),
           rRatioRect.right,rRatioRect.bottom);
 #ifdef DISP_DEBUG
     //sprintf(txtbuffer, "glScissor2 %d %d %d %d\r\n", rRatioRect.left, iResY-(rRatioRect.top+rRatioRect.bottom), rRatioRect.right, rRatioRect.bottom);
     //DEBUG_print(txtbuffer, DBG_CORE3);
     //writeLogFile(txtbuffer);
     #endif // DISP_DEBUG
 glViewport(rRatioRect.left,
           iResY-(rRatioRect.top+rRatioRect.bottom),
           rRatioRect.right,rRatioRect.bottom);
 glError();
}

