/***************************************************************************
    drawGX.m
    PeopsSoftGPU for cubeSX/wiiSX

    Created by sepp256 on Thur Jun 26 2008.
    Copyright (c) 2008 Gil Pedersen.
    Adapted from draw.c by Pete Bernet and drawgl.m by Gil Pedersen.
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
#include <malloc.h>
#include <time.h>
#include <ogc/lwp_watchdog.h>
#include <ogc/machine/processor.h>
#include <wiiuse/wpad.h>
#include "../coredebug.h"
#include "../gpulib/stdafx.h"
//#define _IN_DRAW
#include "externals.h"
#include "gpu.h"
#include "draw.h"
#include "../Gamecube/libgui/IPLFontC.h"
#include "../Gamecube/DEBUG.h"
#include "../Gamecube/wiiSXconfig.h"
#include "../Gamecube/gc_input/controller.h"
#include "../psxcounters.h"
#include "../gpulib/plugin_lib.h"

////////////////////////////////////////////////////////////////////////////////////
// misc globals
////////////////////////////////////////////////////////////////////////////////////
int            iResX;
int            iResY;
//long           lLowerpart;
//BOOL           bCheckMask=FALSE;
//unsigned short sSetMask=0;
//unsigned long  lSetMask=0;
int            iDesktopCol=16;
int            iShowFPS=1;
int            iUseNoStretchBlt=0;
int            iFastFwd=0;
int            iDebugMode=0;
int            iFVDisplay=0;
PSXPoint_t     ptCursorPoint[8];
unsigned short usCursorActive=0;
int            backFromMenu=0;

//Some GX specific variables
#define FB_MAX_SIZE (640 * 528 * 4)

#define RESX_MAX 1024	//Vmem width
#define RESY_MAX 512	//Vmem height
#define GXRESX_MAX 1366	//1024 * 1.33 for ARGB
int		iResX_Max=RESX_MAX;
int		iResY_Max=RESY_MAX;

unsigned char	GXtexture[GXRESX_MAX*RESY_MAX*2] __attribute__((aligned(32)));
extern u32* xfb[3];	/*** Framebuffers ***/
char *	pCaptionText;

extern char text[DEBUG_TEXT_HEIGHT][DEBUG_TEXT_WIDTH]; /*** DEBUG textbuffer ***/
extern char menuActive;
extern char screenMode;
char fpsInfo[32];

// prototypes
static void GX_Flip(const void *buffer, int pitch, u8 fmt,
		    int x, int y, int width, int height);
void drawLine(float x1, float y1, float x2, float y2, char r, char g, char b);
void drawCircle(int x, int y, int radius, int numSegments, char r, char g, char b);

void switchToTVMode(short dWidth, short dHeight, bool retMenu);

static int vsync_enable;
static int new_frame;

enum {
	FB_BACK,
	FB_NEXT,
	FB_FRONT,
};

static void gc_vout_vsync(unsigned int)
{
	u32 *tmp;

	if (new_frame) {
		tmp = xfb[FB_FRONT];
		xfb[FB_FRONT] = xfb[FB_NEXT];
		xfb[FB_NEXT] = tmp;
		new_frame = 0;

		VIDEO_SetNextFramebuffer(xfb[FB_FRONT]);
		VIDEO_Flush();
	}
}

static void gc_vout_copydone(void)
{
	u32 *tmp;

	tmp = xfb[FB_NEXT];
	xfb[FB_NEXT] = xfb[FB_BACK];
	xfb[FB_BACK] = tmp;

	new_frame = 1;
}

static void gc_vout_drawdone(void)
{
	GX_CopyDisp(xfb[FB_BACK], GX_TRUE);
	GX_SetDrawDoneCallback(gc_vout_copydone);
	GX_SetDrawDone();
}

void gc_vout_render(void)
{
	// reset swap table from GUI/DEBUG
	GX_SetTevSwapModeTable(GX_TEV_SWAP0, GX_CH_BLUE, GX_CH_GREEN, GX_CH_RED ,GX_CH_ALPHA);
	GX_SetTevSwapMode(GX_TEVSTAGE0, GX_TEV_SWAP0, GX_TEV_SWAP0);

	GX_SetDrawDoneCallback(gc_vout_drawdone);
	GX_SetDrawDone();
}

void gx_vout_render(void)
{
	// reset swap table from GUI/DEBUG
	// To improve efficiency, the original BGR pixel format of PS is directly used
	// So the format of SwapModeTable is GX_CH_BLUE, GX_CH_GREEN, GX_CH_RED, GX_CH_ALPHA
	GX_SetTevSwapModeTable(GX_TEV_SWAP0, GX_CH_BLUE, GX_CH_GREEN, GX_CH_RED, GX_CH_ALPHA);
	GX_SetTevSwapMode(GX_TEVSTAGE0, GX_TEV_SWAP0, GX_TEV_SWAP0);

	GX_DrawDone();
	GX_CopyDisp(xfb[FB_BACK], GX_TRUE);
	GX_Flush();

	GX_DrawDone();
	GX_Flush();

	gc_vout_copydone();
}

void gx_vout_clear(void)
{
	GX_DrawDone();
	GX_SetCopyClear((GXColor){0, 0, 0, 0xFF}, GX_MAX_Z24);
	GX_CopyDisp(xfb[FB_BACK], GX_TRUE);
	GX_Flush();

	GX_DrawDone();
	GX_Flush();

	new_frame = 1;
	gc_vout_copydone();
	gc_vout_vsync(0);
}

void showFpsAndDebugInfo(void)
{
	//Write menu/debug text on screen
	if (showFPSonScreen == FPS_SHOW)
    {
        GXColor fontColor = {150,255,150,255};
	    IplFont_drawInit(fontColor);
		IplFont_drawString(10,35,fpsInfo, 1.0, false);
		#ifdef SHOW_DEBUG
		int i = 0;
        DEBUG_update();
        for (i=0;i<DEBUG_TEXT_HEIGHT;i++)
		{
            IplFont_drawString(10,(24*i+60),text[i], 0.5, false);
		}
		#endif // SHOW_DEBUG
    }
}

////////////////////////////////////////////////////////////////////////
static int screen_w, screen_h;

static void GX_Flip(const void *buffer, int pitch, u8 fmt,
		    int x, int y, int width, int height)
{
	int h, w, i;
	char r, g, b;
	static int oldwidth=0;
	static int oldheight=0;
	static int oldformat=-1;
	static GXTexObj GXtexobj;
	WPADData* wpad;
	int cursorX;
	int cursorY;

	if((width == 0) || (height == 0))
		return;


	if ((oldwidth != width) || (oldheight != height) || (oldformat != fmt))
	{ //adjust texture conversion
		oldwidth = width;
		oldheight = height;
		oldformat = fmt;
		memset(GXtexture,0,sizeof(GXtexture));
		GX_InitTexObj(&GXtexobj, GXtexture, width, height, fmt, GX_CLAMP, GX_CLAMP, GX_FALSE);
	}

	if (originalMode == ORIGINALMODE_ENABLE || bilinearFilter == BILINEARFILTER_DISABLE)
		GX_InitTexObjFilterMode(&GXtexobj, GX_NEAR, GX_NEAR);
	else
		GX_InitTexObjFilterMode(&GXtexobj, GX_LINEAR, GX_LINEAR);

	if(fmt==GX_TF_RGBA8) {
		vu8 *wgPipePtr = GX_RedirectWriteGatherPipe(GXtexture);
		u8 *image = (u8 *) buffer;
		for (h = 0; h < (height >> 2); h++)
		{
			for (w = 0; w < (width >> 2); w++)
			{
				// Convert from RGB888 to GX_TF_RGBA8 texel format
				for (int texelY = 0; texelY < 4; texelY++)
				{
					for (int texelX = 0; texelX < 4; texelX++)
					{
						// AR
						int pixelAddr = (((w*4)+(texelX))*3) + (texelY*pitch);
						*wgPipePtr = 0xFF;	// A
						*wgPipePtr = image[pixelAddr+2]; // R (which is B in little endian)
					}
				}

				for (int texelY = 0; texelY < 4; texelY++)
				{
					for (int texelX = 0; texelX < 4; texelX++)
					{
						// GB
						int pixelAddr = (((w*4)+(texelX))*3) + (texelY*pitch);
						*wgPipePtr = image[pixelAddr+1]; // G
						*wgPipePtr = image[pixelAddr]; // B (which is R in little endian)
					}
				}
			}
			image+=pitch*4;
		}
	}
	else {
		vu32 *wgPipePtr = GX_RedirectWriteGatherPipe(GXtexture);
		u32 *src1 = (u32 *) buffer;
		u32 *src2 = (u32 *) (buffer + pitch);
		u32 *src3 = (u32 *) (buffer + (pitch * 2));
		u32 *src4 = (u32 *) (buffer + (pitch * 3));
		int rowpitch = (pitch) - (width >> 1);
		u32 alphaMask = 0x00800080;
		for (h = 0; h < height; h += 4)
		{
			for (w = 0; w < (width >> 2); w++)
			{
				// Convert from BGR555 LE data to BGR BE, we OR the first bit with "1" to send RGB5A3 mode into RGB555 on the GP (GC/Wii)
				__asm__ (
					"lwz 3, 0(%1)\n"
					"lwz 4, 4(%1)\n"
					"lwz 5, 0(%2)\n"
					"lwz 6, 4(%2)\n"

					"rlwinm 3, 3, 16, 0, 31\n"
					"rlwinm 4, 4, 16, 0, 31\n"
					"rlwinm 5, 5, 16, 0, 31\n"
					"rlwinm 6, 6, 16, 0, 31\n"

					"or 3, 3, %5\n"
					"or 4, 4, %5\n"
					"or 5, 5, %5\n"
					"or 6, 6, %5\n"

					"stwbrx 3, 0, %0\n"
					"stwbrx 4, 0, %0\n"
					"stwbrx 5, 0, %0\n"
					"stwbrx 6, 0, %0\n"

					"lwz 3, 0(%3)\n"
					"lwz 4, 4(%3)\n"
					"lwz 5, 0(%4)\n"
					"lwz 6, 4(%4)\n"

					"rlwinm 3, 3, 16, 0, 31\n"
					"rlwinm 4, 4, 16, 0, 31\n"
					"rlwinm 5, 5, 16, 0, 31\n"
					"rlwinm 6, 6, 16, 0, 31\n"

					"or 3, 3, %5\n"
					"or 4, 4, %5\n"
					"or 5, 5, %5\n"
					"or 6, 6, %5\n"

					"stwbrx 3, 0, %0\n"
					"stwbrx 4, 0, %0\n"
					"stwbrx 5, 0, %0\n"
					"stwbrx 6, 0, %0"
					: : "r" (wgPipePtr), "r" (src1), "r" (src2), "r" (src3), "r" (src4), "r" (alphaMask) : "memory", "r3", "r4", "r5", "r6");
					//         %0             %1            %2          %3          %4           %5
				src1+=2;
				src2+=2;
				src3+=2;
				src4+=2;
			}

		  src1 += rowpitch;
		  src2 += rowpitch;
		  src3 += rowpitch;
		  src4 += rowpitch;
		}
	}
	GX_RestoreWriteGatherPipe();

	GX_LoadTexObj(&GXtexobj, GX_TEXMAP0);

	Mtx44	GXprojIdent;
	Mtx		GXmodelIdent;
	guMtxIdentity(GXprojIdent);
	guMtxIdentity(GXmodelIdent);
	GXprojIdent[2][2] = 0.5;
	GXprojIdent[2][3] = -0.5;
	GX_LoadProjectionMtx(GXprojIdent, GX_ORTHOGRAPHIC);
	GX_LoadPosMtxImm(GXmodelIdent,GX_PNMTX0);

	GX_SetCullMode(GX_CULL_NONE);
	GX_SetZMode(GX_ENABLE,GX_ALWAYS,GX_TRUE);

	GX_InvalidateTexAll();
	GX_SetTevOp(GX_TEVSTAGE0, GX_REPLACE);
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLORNULL);


	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_PTNMTXIDX, GX_PNMTX0);
	GX_SetVtxDesc(GX_VA_TEX0MTXIDX, GX_IDENTITY);
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);

	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XY, GX_F32, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

	GX_SetNumTexGens(1);
	GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);

	float xcoord = 1.0;
	float ycoord = 1.0;
	if(screenMode == SCREENMODE_16x9_PILLARBOX) xcoord = 640.0/848.0;

	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
	  GX_Position2f32(-xcoord, ycoord);
	  GX_TexCoord2f32( 0.0, 0.0);
	  GX_Position2f32( xcoord, ycoord);
	  GX_TexCoord2f32( 1.0, 0.0);
	  GX_Position2f32( xcoord,-ycoord);
	  GX_TexCoord2f32( 1.0, 1.0);
	  GX_Position2f32(-xcoord,-ycoord);
	  GX_TexCoord2f32( 0.0, 1.0);
	GX_End();

	if (lightGun){

	GX_InvVtxCache();
	GX_InvalidateTexAll();
	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS,	GX_POS_XY,	GX_F32,	0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8,	0);
	GX_SetNumChans(1);
	GX_SetNumTexGens(0);
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);
	GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
	GX_SetLineWidth(10, GX_TO_ZERO );

		for (i=0;i<4;i++){
			if (controller_Wiimote.available[i] || controller_WiimoteNunchuk.available[i]){
				wpad = WPAD_Data(0);
				if (!wpad[i].ir.valid) continue;
				cursorX = wpad[i].ir.x;
				cursorY = wpad[i].ir.y;
				switch (i){
					case 0: r=0; g=255; b=0;break;
					case 1: r=255; g=0; b=0;break;
					case 2: r=0; g=0; b=255;break;
					case 3: r=0; g=255; b=255;break;
				}

				drawLine(cursorX, cursorY-5, cursorX, cursorY+5, r, g, b);
				drawLine(cursorX-5, cursorY, cursorX+5, cursorY, r, g, b);

				if (lightGun == LIGHTGUN_GUNCON || lightGun == LIGHTGUN_JUST){
					drawCircle(cursorX, cursorY, 20, 10, r, g, b);
					drawLine(cursorX+20, cursorY, cursorX+15, cursorY, r, g, b);
					drawLine(cursorX-20, cursorY, cursorX-15, cursorY, r, g, b);
					drawLine(cursorX, cursorY+20, cursorX, cursorY+15, r, g, b);
					drawLine(cursorX, cursorY-20, cursorX, cursorY-15, r, g, b);
				}
			}
		}
	}

	//Write menu/debug text on screen
	showFpsAndDebugInfo();

	gc_vout_render();
}

drawCircle(int x, int y, int radius, int numSegments, char r, char g, char b)
{

	GX_Begin(GX_LINESTRIP, GX_VTXFMT0, numSegments+1);

	for (int i = 0; i<=numSegments; i++)
	{
		float angle, point_x, point_y;
		angle = 2*3.14 * i/numSegments;
		point_x = (float)x + (float)radius * cos( angle );
		point_y = (float)y + (float)radius * sin( angle );

		point_x = (point_x/320)-1;
		point_y = ((point_y/240)-1)*-1;

		GX_Position2f32((float) point_x,(float) point_y);
		GX_Color4u8(r, g, b, 0xFF);
	}

	GX_End();
}

drawLine(float x1, float y1, float x2, float y2, char r, char g, char b)
{
	x1 = (x1/320)-1;
	y1 = ((y1/240)-1) *-1;
	x2 = (x2/320)-1;
	y2 = ((y2/240)-1) *-1;
	GX_Begin(GX_LINES, GX_VTXFMT0, 2);
		GX_Position2f32((float) x1,(float) y1);
		GX_Color4u8(r, g, b, 0xFF);
		GX_Position2f32((float) x2,(float) y2);
		GX_Color4u8(r, g, b, 0xFF);
	GX_End();
}

// =============================================================================================

int gc_vout_open(void) {
	memset(GXtexture,0,sizeof(GXtexture));
	VIDEO_SetPreRetraceCallback(gc_vout_vsync);
	return 0;
}

int gx_vout_open(void) {
	VIDEO_SetPreRetraceCallback(gc_vout_vsync);
	return 0;
}

static void gc_vout_close(void) {}

static void gc_vout_flip(const void *vram, int stride, int bgr24,
			      int x, int y, int w, int h, int dims_changed) {
	if (vram == NULL) {
		memset(GXtexture,0,sizeof(GXtexture));
		if (menuActive) return;

		// Write menu/debug text on screen
		if (showFPSonScreen == FPS_SHOW)
		{
			GXColor fontColor = {150,255,150,255};
			IplFont_drawInit(fontColor);
			IplFont_drawString(10,35,fpsInfo, 1.0, false);
			#ifdef SHOW_DEBUG
			int i = 0;
			DEBUG_update();
			for (i=0;i<DEBUG_TEXT_HEIGHT;i++)
			{
				IplFont_drawString(10,(24*i+60),text[i], 0.5, false);
			}
			#endif // SHOW_DEBUG
		}

		gc_vout_render();
		return;
	}
	if (menuActive) return;

	GX_Flip(vram, stride * 2, bgr24 ? GX_TF_RGBA8 : GX_TF_RGB5A3, x, y, w, h);
}

// For old softGpu
void DoBufferSwap(void)                                // SWAP BUFFERS
{                                                      // (we don't swap... we blit only)
	static int iOldDX=0;
	static int iOldDY=0;
	long x = PSXDisplay.DisplayPosition.x;
	long y = PSXDisplay.DisplayPosition.y;
	short iDX = PreviousPSXDisplay.Range.x1 & 0xFFF8;
	if (iDX < PreviousPSXDisplay.Range.x1)
		iDX += 8;
	short iDY = PreviousPSXDisplay.DisplayMode.y;

	if (menuActive) return;

	if(iOldDX!=iDX || iOldDY!=iDY)
	{
		memset(GXtexture, 0, sizeof(GXtexture));
		iOldDX=iDX;iOldDY=iDY;
		backFromMenu = 1;
	}

//	if (showFPSonScreen == FPS_SHOW)
//	{
//		if(szDebugText[0] && ((time(NULL) - tStart) < 2))
//		{
//			strcpy(szDispBuf,szDebugText);
//		}
//		else
//		{
//			szDebugText[0]=0;
//			strcat(szDispBuf,szMenuBuf);
//		}
//	}

	u8 *imgPtr = (u8*)(psxVuw + ((y<<10) + x));
	if(PSXDisplay.RGB24) {
		long lPitch=iResX_Max<<1;

		if(PreviousPSXDisplay.Range.y0)                       // centering needed?
		{
			imgPtr+=PreviousPSXDisplay.Range.y0*lPitch;
			iDY-=PreviousPSXDisplay.Range.y0;
		}
		imgPtr+=PreviousPSXDisplay.Range.x0<<1;
	}


	GX_Flip(imgPtr, iResX_Max*2, PSXDisplay.RGB24 ? GX_TF_RGBA8 : GX_TF_RGB5A3, 0, 0, iDX, iDY);

	// Check if TVMode needs to be changed (240 or 480 lines)
	if (originalMode == ORIGINALMODE_ENABLE)
	{
		if(backFromMenu)
		{
			backFromMenu = 0;
			switchToTVMode(iDX, iDY, 0);
		}
	}
}

// For old softGpu
void DoClearFrontBuffer(void)                          // CLEAR DX BUFFER
{
	if (menuActive) return;

	//Write menu/debug text on screen
	if (showFPSonScreen == FPS_SHOW)
	{
	    GXColor fontColor = {150,255,150,255};
        IplFont_drawInit(fontColor);
		IplFont_drawString(10,35,fpsInfo, 1.0, false);
		#ifdef SHOW_DEBUG
		int i = 0;
        DEBUG_update();
        for (i=0;i<DEBUG_TEXT_HEIGHT;i++)
		{
            IplFont_drawString(10,(24*i+60),text[i], 0.5, false);
		}
		#endif // SHOW_DEBUG
	}

    gc_vout_render();
}

static void gc_vout_set_mode(int w, int h, int raw_w, int raw_h, int bpp) {
    screen_w = raw_w;
    screen_h = raw_h;

    if (menuActive) return;

    // Check if TVMode needs to be changed (240 or 480 lines)
    if (originalMode == ORIGINALMODE_ENABLE)
    {
        backFromMenu = 0;
        switchToTVMode(w, h, 0);
    }
}

extern u32 hSyncCount;
extern u32 frame_counter;
extern void gpu_state_change(int what);

static struct rearmed_cbs gc_rearmed_cbs = {
   .pl_vout_open     = gc_vout_open,
   .pl_vout_set_mode = gc_vout_set_mode,
   .pl_vout_flip     = gc_vout_flip,
   .pl_vout_close    = gc_vout_close,
   //.mmap             = gc_mmap,
   //.munmap           = gc_munmap,
   /* from psxcounters */
   .gpu_hcnt         = &hSyncCount,
   .gpu_frame_count  = &frame_counter,
   .gpu_state_change = gpu_state_change,

   .gpu_unai = {
	   .lighting = 1,
	   .blending = 1,
   },
};

// Frame limiting/calculation routines, taken from plugin_lib.c, adapted for Wii/GC.
extern int g_emu_resetting;
static int vsync_cnt;
static int is_pal, frame_interval, frame_interval1024;
static int vsync_usec_time;
#define MAX_LAG_FRAMES 3

#define tvdiff(tv, tv_old) \
	((tv.tv_sec - tv_old.tv_sec) * 1000000 + tv.tv_usec - tv_old.tv_usec)

#define       MAXSKIP 120
#define       MAXLACE 16
static DWORD  dwLaceCnt = 0;
static BOOL   bInitCap = TRUE;
static float  fps_skip = 0;
static float  fps_cur  = 0;

static unsigned long timeGetTime()
{
    long long nowTick = gettime();
    return diff_usec(0, nowTick) / 10;
}

static void FrameCap (void)
{
    static unsigned long curticks, lastticks, _ticks_since_last_update;
    static unsigned long TicksToWait = 0;

    curticks = timeGetTime();
    _ticks_since_last_update = curticks - lastticks;

    if ((_ticks_since_last_update > TicksToWait) ||
            (curticks <lastticks))
    {
        lastticks = curticks;

        if ((_ticks_since_last_update-TicksToWait) > gc_rearmed_cbs.gpu_peops.dwFrameRateTicks)
            TicksToWait = 0;
        else TicksToWait = gc_rearmed_cbs.gpu_peops.dwFrameRateTicks - (_ticks_since_last_update - TicksToWait);
    }
    else
    {
        BOOL Waiting = TRUE;
        while (Waiting)
        {
            curticks = timeGetTime();
            _ticks_since_last_update = curticks - lastticks;
            if ((_ticks_since_last_update > TicksToWait) ||
                    (curticks < lastticks))
            {
                Waiting = FALSE;
                lastticks = curticks;
                TicksToWait = gc_rearmed_cbs.gpu_peops.dwFrameRateTicks;
            }
        }
    }
}

static void CalcFps(void)
{
    static unsigned long _ticks_since_last_update;
    static unsigned long fps_cnt = 0;
    static unsigned long fps_tck = 1;
    {
        static unsigned long lastticks;
        static unsigned long curticks;

        curticks = timeGetTime();
        _ticks_since_last_update = curticks - lastticks;

        if (gc_rearmed_cbs.frameskip && (frameLimit[0] != FRAMELIMIT_AUTO) && _ticks_since_last_update)
            fps_skip = min(fps_skip, ((float)TIMEBASE / (float)_ticks_since_last_update + 1.0f));

        lastticks = curticks;
    }

    if (gc_rearmed_cbs.frameskip && (frameLimit[0] == FRAMELIMIT_AUTO))
    {
        static unsigned long fpsskip_cnt = 0;
        static unsigned long fpsskip_tck = 1;

        fpsskip_tck += _ticks_since_last_update;

        if (++fpsskip_cnt == 2)
        {
            fps_skip = (float)2000 / (float)fpsskip_tck;
            fps_skip += 6.0f;
            fpsskip_cnt = 0;
            fpsskip_tck = 1;
        }
    }

    fps_tck += _ticks_since_last_update;

    if (++fps_cnt == 10)
    {
        fps_cur = (float)(TIMEBASE * 10) / (float)fps_tck;

        fps_cnt = 0;
        fps_tck = 1;

        if ((frameLimit[0] == FRAMELIMIT_AUTO) && fps_cur > gc_rearmed_cbs.gpu_peops.fFrameRateHz)    // optical adjust ;) avoids flickering fps display
            fps_cur = gc_rearmed_cbs.gpu_peops.fFrameRateHz;
    }

    sprintf(fpsInfo, "FPS %.2f", fps_cur);
}

void CheckFrameRate(void)
{
//    if (gc_rearmed_cbs.frameskip)                           // skipping mode?
//    {
//        dwLaceCnt++;                                     // -> store cnt of vsync between frames
//        if (dwLaceCnt >= MAXLACE && frameLimit[0] == FRAMELIMIT_AUTO)       // -> if there are many laces without screen toggling,
//        {                                                //    do std frame limitation
//            if (dwLaceCnt == MAXLACE) bInitCap = TRUE;
//
//            FrameCap();
//        }
//        CalcFps();        // -> calc fps display in skipping mode
//        if (fps_cur < gc_rearmed_cbs.gpu_peops.fFrameRateHz)
//            gc_rearmed_cbs.fskip_advice = 1;
//        else
//            gc_rearmed_cbs.fskip_advice = 0;
//    }
//    else                                                  // non-skipping mode:
    {
        if (frameLimit[0] == FRAMELIMIT_AUTO) FrameCap();                      // -> do it
        if (showFPSonScreen == FPS_SHOW) CalcFps();          // -> and calc fps display
    }
}

/* called on every vsync */
void pl_frame_limit(void)
{
	static struct timeval tv_old, tv_expect;
	static int vsync_cnt_prev, drc_active_vsyncs;
	struct timeval now;
	int diff, usadj;

	if (g_emu_resetting)
		return;

    // used by P.E.Op.S. frame limit code
    CheckFrameRate();

//	vsync_cnt++;
//	gettimeofday(&now, 0);
//
//	if (now.tv_sec != tv_old.tv_sec) {
//		diff = tvdiff(now, tv_old);
//		gc_rearmed_cbs.vsps_cur = 0.0f;
//		if (0 < diff && diff < 2000000)
//			gc_rearmed_cbs.vsps_cur = 1000000.0f * (vsync_cnt - vsync_cnt_prev) / diff;
//		vsync_cnt_prev = vsync_cnt;
//
//		gc_rearmed_cbs.flips_per_sec = gc_rearmed_cbs.flip_cnt;
//		sprintf(fpsInfo, "FPS %.2f", gc_rearmed_cbs.vsps_cur);
//
//		gc_rearmed_cbs.flip_cnt = 0;
//
//		tv_old = now;
//		//new_dynarec_print_stats();
//	}
//
//	// tv_expect uses usec*1024 units instead of usecs for better accuracy
//	tv_expect.tv_usec += frame_interval1024;
//	if (tv_expect.tv_usec >= (1000000 << 10)) {
//		tv_expect.tv_usec -= (1000000 << 10);
//		tv_expect.tv_sec++;
//	}
//	diff = (tv_expect.tv_sec - now.tv_sec) * 1000000 + (tv_expect.tv_usec >> 10) - now.tv_usec;
//
//	if (diff > MAX_LAG_FRAMES * frame_interval || diff < -MAX_LAG_FRAMES * frame_interval) {
//		//printf("pl_frame_limit reset, diff=%d, iv %d\n", diff, frame_interval);
//		tv_expect = now;
//		diff = 0;
//		// try to align with vsync
//		usadj = vsync_usec_time;
//		while (usadj < tv_expect.tv_usec - frame_interval)
//			usadj += frame_interval;
//		tv_expect.tv_usec = usadj << 10;
//	}
//
//	if (frameLimit[0] == FRAMELIMIT_AUTO && diff > frame_interval) {
//		//if (vsync_enable)
//		//	VIDEO_WaitVSync();
//		//else
//			usleep(diff - frame_interval);
//	}

//	if (gc_rearmed_cbs.frameskip) {
//		if (diff < -frame_interval)
//			gc_rearmed_cbs.fskip_advice = 1;
//		else if (diff >= 0)
//			gc_rearmed_cbs.fskip_advice = 0;
//	}
}

double max_vid_fps;

void pl_timing_prepare(int is_pal_)
{
	double max_vid_fps;
	gc_rearmed_cbs.fskip_advice = 0;
	gc_rearmed_cbs.flips_per_sec = 0;
	gc_rearmed_cbs.cpu_usage = 0;

    pl_chg_psxtype(is_pal_);
}

void pl_chg_psxtype(int is_pal_)
{
	is_pal = is_pal_;
	max_vid_fps = psxGetFps();
	frame_interval = (int)(1000000.0 / max_vid_fps);
	frame_interval1024 = (int)(1000000.0 * 1024.0 / max_vid_fps);

	// used by P.E.Op.S. frame limit code
	if (PSXDisplay.Interlaced)
	{
		gc_rearmed_cbs.gpu_peops.fFrameRateHz = is_pal ? 50.00238f : 59.94146f;
	}
	else
	{
		gc_rearmed_cbs.gpu_peops.fFrameRateHz = (float)max_vid_fps;
	}

	gc_rearmed_cbs.gpu_peops.dwFrameRateTicks =
		(int) (TIMEBASE / gc_rearmed_cbs.gpu_peops.fFrameRateHz);

	//vsync_enable = !is_pal && frameLimit[0] == FRAMELIMIT_AUTO;
}

void plugin_call_rearmed_cbs(unsigned long autoDwActFixes, int cfgUseDithering)
{
	gc_rearmed_cbs.screen_centering_type_default =
		Config.hacks.gpu_centering ? 1 : 0;

	extern void LIB_GPUrearmedCallbacks(const struct rearmed_cbs *cbs);
	gc_rearmed_cbs.gpu_peops.dwActFixes = autoDwActFixes;
	gc_rearmed_cbs.gpu_peops.iUseDither = cfgUseDithering;
	gc_rearmed_cbs.frameskip = frameSkip;

	LIB_GPUrearmedCallbacks(&gc_rearmed_cbs);
}
