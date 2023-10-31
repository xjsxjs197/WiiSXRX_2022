/**
 * WiiSX - GraphicsGX.cpp
 * Copyright (C) 2009, 2010 sepp256
 *
 * WiiSX homepage: http://www.emulatemii.com
 * email address: sepp256@gmail.com
 *
 *
 * This program is free software; you can redistribute it and/
 * or modify it under the terms of the GNU General Public Li-
 * cence as published by the Free Software Foundation; either
 * version 2 of the Licence, or any later version.
 *
 * This program is distributed in the hope that it will be use-
 * ful, but WITHOUT ANY WARRANTY; without even the implied war-
 * ranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public Licence for more details.
 *
**/

#include <math.h>
#include "GraphicsGX.h"

GXRModeObj TVMODE_240p =
{
	VI_TVMODE_EURGB60_DS, 	// viDisplayMode
	640, 					// fbWidth
	240, 					// efbHeight
	240, 					// xfbHeight
	(VI_MAX_WIDTH_NTSC - 640)/2, 		// viXOrigin
	(VI_MAX_HEIGHT_NTSC/2 - 480/2)/2, 	// viYOrigin
	640, 					// viWidth
	480, 					// viHeight
	VI_XFBMODE_SF, 			// xFBmode
	GX_FALSE, 				// field_rendering
	GX_FALSE, 				// aa
	// sample points arranged in increasing Y order
	{
		{6,6},{6,6},{6,6},  // pix 0, 3 sample points, 1/12 units, 4 bits each
		{6,6},{6,6},{6,6},  // pix 1
		{6,6},{6,6},{6,6},  // pix 2
		{6,6},{6,6},{6,6}   // pix 3
	},
	// vertical filter[7], 1/64 units, 64=1.0
	{
		0, 		// line n-1
		0, 		// line n-1
		21, 	// line n
		22, 	// line n
		21, 	// line n+1
		0, 		// line n+1
		0 		// line n+2
	}
};
extern "C" unsigned int usleep(unsigned int us);
void video_mode_init(GXRModeObj *rmode, u32 *fb1, u32 *fb2, u32 *fb3);
extern "C" void VIDEO_SetTrapFilter(bool enable);

GXRModeObj gvmode;
GXRModeObj mgvmode;

extern u32* xfb[3];
enum {
	FB_BACK,
	FB_NEXT,
	FB_FRONT,
};

extern "C" void switchToTVMode(short dWidth, short dHeight, bool retMenu){
	GXRModeObj *rmode;
	f32 yscale;
	u32 xfbHeight;
    u16 xfbWidth;
	u32 width = 0;
	u32 height = 0;
	Mtx44 perspective;
	GXColor background = {0, 0, 0, 0xff};
	if (dWidth <= 320)
		dWidth *= 2;

	rmode = &mgvmode;
	if (dWidth > rmode->viWidth)
		dWidth = rmode->viWidth;
	if (dHeight > rmode->viHeight)
		dHeight = rmode->viHeight;


	if ((dHeight > 288))
	{
		if(!retMenu)
		{
			rmode = &gvmode;
			rmode->fbWidth = dWidth;
			rmode->efbHeight = dHeight;
			rmode->xfbHeight = dHeight;
			rmode->viYOrigin = (VI_MAX_HEIGHT_NTSC - dHeight)/2;
			if ((CONF_GetEuRGB60() == 0) && (CONF_GetVideo() == CONF_VIDEO_PAL))
				rmode->viYOrigin = (VI_MAX_HEIGHT_PAL - dHeight)/2;

			if (interlacedMode){
				rmode->viTVMode =	(mgvmode.viTVMode & ~0x03) + VI_INTERLACE;
				rmode->xfbMode = VI_XFBMODE_DF;
			}
			else{
				rmode->viTVMode =	mgvmode.viTVMode;
				rmode->xfbMode = mgvmode.xfbMode;
			}
		}
		xfbWidth = VIDEO_PadFramebufferWidth(rmode->fbWidth);
		width = xfbWidth;
	}
	else
	{
		if (CONF_GetVideo() == CONF_VIDEO_NTSC)
		{
			rmode = &TVMODE_240p;
			rmode->viTVMode = VI_TVMODE_NTSC_DS;
			rmode->viYOrigin = (VI_MAX_HEIGHT_NTSC/2 - dHeight)/2;

		}
		else if (CONF_GetEuRGB60() > 0)
		{
			rmode = &TVMODE_240p;
			rmode->viYOrigin = (VI_MAX_HEIGHT_NTSC/2 - dHeight)/2;
		}
		else if (CONF_GetVideo() == CONF_VIDEO_MPAL)
		{
			rmode = &TVMODE_240p;
			rmode->viTVMode = VI_TVMODE_MPAL_DS;
			rmode->viYOrigin = (VI_MAX_HEIGHT_NTSC/2 - dHeight)/2;
		}
		else
		{
			rmode = &TVMODE_240p;
			rmode->viTVMode = VI_TVMODE_PAL_DS;
			rmode->viXOrigin = (VI_MAX_WIDTH_PAL - 640)/2;
			rmode->viYOrigin = (VI_MAX_HEIGHT_PAL/2 - dHeight)/2;
		}
		rmode->efbHeight = dHeight;
		rmode->xfbHeight = dHeight;

		rmode->fbWidth = dWidth;
		width = rmode->fbWidth;

	}

	GX_SetCopyFilter(rmode->aa,rmode->sample_pattern,(deflickerFilter)?GX_TRUE:GX_FALSE,rmode->vfilter);

	VIDEO_Configure (rmode);
	VIDEO_Flush();
	if (retMenu){
		GX_SetCopyFilter(rmode->aa,rmode->sample_pattern,GX_TRUE,rmode->vfilter);
		VIDEO_ClearFrameBuffer (rmode, xfb[FB_FRONT], COLOR_BLACK);
		VIDEO_Flush ();
	}
	VIDEO_WaitVSync();
	if(rmode->viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();

	GX_SetCopyClear(background, 0x00ffffff);
	GX_SetViewport(0.0F, 0.0F, rmode->fbWidth, rmode->efbHeight, 0.0F, 1.0F);
	yscale = GX_GetYScaleFactor(rmode->efbHeight,rmode->xfbHeight);
	xfbHeight = GX_SetDispCopyYScale(yscale);
	xfbWidth = VIDEO_PadFramebufferWidth(rmode->fbWidth);
	GX_SetScissor(0,0,rmode->fbWidth,rmode->efbHeight);
	GX_SetPixelFmt(GX_PF_RGB8_Z24, GX_ZC_LINEAR);
	GX_SetDispCopySrc(0,0,rmode->fbWidth,rmode->efbHeight);
	GX_SetDispCopyDst(xfbWidth, xfbHeight);
	GX_SetFieldMode(rmode->field_rendering,((rmode->viHeight==2*rmode->xfbHeight)?GX_ENABLE:GX_DISABLE));
	GX_SetCullMode(GX_CULL_NONE);
	GX_SetDispCopyGamma(GX_GM_1_0);
	GX_SetNumChans(1);
	GX_SetNumTexGens(1);
	GX_SetTevOp(GX_TEVSTAGE0, GX_MODULATE);
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
	GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
	height = rmode->efbHeight;
	guOrtho(perspective,0,height,0,width,0,300);
	GX_LoadProjectionMtx(perspective, GX_ORTHOGRAPHIC);
	GX_SetDither(GX_FALSE);

}


namespace menu {

Graphics::Graphics(GXRModeObj *rmode)
		: vmode(rmode),
		  which_fb(0),
		  first_frame(true),
		  depth(1.0f),
		  transparency(1.0f),
		  viewportWidth(640.0f),
		  viewportHeight(480.0f)
{
//	printf("Graphics constructor\n");

	setColor((GXColor) {0,0,0,0});

#ifdef HW_RVL
	CONF_Init();
#endif
	VIDEO_Init();
	switch (videoMode)
	{
	case VIDEOMODE_AUTO:
		//vmode = VIDEO_GetPreferredMode(NULL);
		vmode = VIDEO_GetPreferredMode(&vmode_phys);
#if 1
		if(CONF_GetAspectRatio()) {
			vmode->viWidth = 678;
			vmode->viXOrigin = (VI_MAX_WIDTH_PAL - 678) / 2;
		}
#endif
		if (memcmp( &vmode_phys, &TVPal528IntDf, sizeof(GXRModeObj)) == 0)
			memcpy( &vmode_phys, &TVPal576IntDfScale, sizeof(GXRModeObj));
		break;
	case VIDEOMODE_NTSC:
		vmode = &TVNtsc480IntDf;
		memcpy( &vmode_phys, vmode, sizeof(GXRModeObj));
		break;
	case VIDEOMODE_PAL50:
		vmode = &TVPal576IntDfScale;
		memcpy( &vmode_phys, vmode, sizeof(GXRModeObj));
		break;
	case VIDEOMODE_PAL60:
		vmode = &TVEurgb60Hz480IntDf;
		memcpy( &vmode_phys, vmode, sizeof(GXRModeObj));
		break;
	case VIDEOMODE_PROGRESSIVE:
		vmode = &TVNtsc480Prog;
		memcpy( &vmode_phys, vmode, sizeof(GXRModeObj));
		break;
	}

	//vmode->efbHeight = viewportHeight; // Note: all possible modes have efbHeight of 480
	memcpy( &gvmode, vmode, sizeof(GXRModeObj));
	memcpy( &mgvmode, vmode, sizeof(GXRModeObj));
	if (trapFilter)
		VIDEO_SetTrapFilter(1);

	VIDEO_Configure(vmode);

	xfb[0] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(vmode));
	xfb[1] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(vmode));
	xfb[2] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(vmode));

	console_init (xfb[0], 20, 64, vmode->fbWidth, vmode->xfbHeight, vmode->fbWidth * 2);

	VIDEO_SetNextFramebuffer(xfb[which_fb]);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if(vmode->viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();
	which_fb ^= 1;

	//Pass vmode, xfb[0] and xfb[1] back to main program
	video_mode_init(vmode, (u32*)xfb[0], (u32*)xfb[1], (u32*)xfb[2]);

	//Perform GX init stuff here?
	//GX_init here or in main?
	//GX_SetViewport( 0.0f, 0.0f, viewportWidth, viewportHeight );
	init();
}

Graphics::~Graphics()
{
}

void Graphics::init()
{

	f32 yscale;
	u32 xfbHeight;
	void *gpfifo = NULL;
	GXColor background = {0, 0, 0, 0xff};

	gpfifo = memalign(32,GX_FIFO_MINSIZE);
	memset(gpfifo,0,GX_FIFO_MINSIZE);
	GX_Init(gpfifo,GX_FIFO_MINSIZE);
	GX_SetCopyClear(background, GX_MAX_Z24);

	GX_SetViewport(0,0,vmode->fbWidth,vmode->efbHeight,0,1);
	yscale = GX_GetYScaleFactor(vmode->efbHeight,vmode->xfbHeight);
	xfbHeight = GX_SetDispCopyYScale(yscale);
	GX_SetScissor(0,0,vmode->fbWidth,vmode->efbHeight);
	GX_SetDispCopySrc(0,0,vmode->fbWidth,vmode->efbHeight);
	GX_SetDispCopyDst(vmode->fbWidth,xfbHeight);
	GX_SetCopyFilter(vmode->aa,vmode->sample_pattern,GX_TRUE,vmode->vfilter);
	GX_SetFieldMode(vmode->field_rendering,((vmode->viHeight==2*vmode->xfbHeight)?GX_ENABLE:GX_DISABLE));

	if (vmode->aa)
			GX_SetPixelFmt(GX_PF_RGB565_Z16, GX_ZC_LINEAR);
		else
			GX_SetPixelFmt(GX_PF_RGB8_Z24, GX_ZC_LINEAR);

	GX_SetCullMode(GX_CULL_NONE);
	GX_SetDispCopyGamma(GX_GM_1_0);

	GX_InvVtxCache();
	GX_InvalidateTexAll();

	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_NRM, GX_NRM_XYZ, GX_F32, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_PTNMTXIDX, GX_PNMTX0);
	GX_SetVtxDesc(GX_VA_TEX0MTXIDX, GX_TEXMTX0);
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);

	setTEV(GX_PASSCLR);
	newModelView();
	loadModelView();
	loadOrthographic();
}

void Graphics::drawInit()
{
	// Reset various parameters from gfx plugin
	GX_SetZTexture(GX_ZT_DISABLE,GX_TF_Z16,0); //GX_ZT_DISABLE or GX_ZT_REPLACE; set in gDP.cpp
	GX_SetZCompLoc(GX_TRUE); // Do Z-compare before texturing.
	GX_SetFog(GX_FOG_NONE,0,1,0,1,(GXColor){0,0,0,255});
	GX_SetViewport(0,0,vmode->fbWidth,vmode->efbHeight,0,1);
	GX_SetCoPlanar(GX_DISABLE);
	GX_SetClipMode(GX_CLIP_ENABLE);
	GX_SetScissor(0,0,vmode->fbWidth,vmode->efbHeight);
	GX_SetAlphaCompare(GX_ALWAYS,0,GX_AOP_AND,GX_ALWAYS,0);

	GX_SetZMode(GX_ENABLE,GX_ALWAYS,GX_TRUE);

	//set blend mode
	GX_SetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR); //Fix src alpha
	GX_SetColorUpdate(GX_ENABLE);
	GX_SetAlphaUpdate(GX_ENABLE);
	GX_SetDstAlpha(GX_DISABLE, 0xFF);
	//set cull mode
	GX_SetCullMode (GX_CULL_NONE);

	GX_InvVtxCache();
	GX_InvalidateTexAll();

	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_NRM, GX_NRM_XYZ, GX_F32, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_PTNMTXIDX, GX_PNMTX0);
	GX_SetVtxDesc(GX_VA_TEX0MTXIDX, GX_TEXMTX0);
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);

	setTEV(GX_PASSCLR);
	newModelView();
	loadModelView();
	loadOrthographic();
}

void Graphics::swapBuffers()
{
//	printf("Graphics swapBuffers\n");
//	if(which_fb==1) usleep(1000000);
	GX_SetCopyClear((GXColor){0, 0, 0, 0xFF}, GX_MAX_Z24);
	GX_CopyDisp(xfb[which_fb],GX_TRUE);
	GX_Flush();

	VIDEO_SetNextFramebuffer(xfb[which_fb]);
	if(first_frame) {
		first_frame = false;
		VIDEO_SetBlack(GX_FALSE);
	}
	VIDEO_Flush();
 	VIDEO_WaitVSync();
	which_fb ^= 1;
//	printf("Graphics endSwapBuffers\n");
}

void Graphics::clearEFB(GXColor color, u32 zvalue)
{
	GX_SetColorUpdate(GX_ENABLE);
	GX_SetAlphaUpdate(GX_ENABLE);
	GX_SetZMode(GX_ENABLE,GX_ALWAYS,GX_TRUE);
	GX_SetCopyClear(color, zvalue);
	GX_CopyDisp(xfb[which_fb],GX_TRUE);
	GX_Flush();
}

void Graphics::newModelView()
{
	guMtxIdentity(currentModelViewMtx);
}

void Graphics::translate(float x, float y, float z)
{
	Mtx tmp;
	guMtxTrans (tmp, x, y, z);
	guMtxConcat (currentModelViewMtx, tmp, currentModelViewMtx);
}

void Graphics::translateApply(float x, float y, float z)
{
	guMtxTransApply(currentModelViewMtx,currentModelViewMtx,x,y,z);
}

void Graphics::rotate(float degrees)
{
	guMtxRotDeg(currentModelViewMtx,'Z',degrees);
}

void Graphics::loadModelView()
{
	GX_LoadPosMtxImm(currentModelViewMtx,GX_PNMTX0);
}

void Graphics::loadOrthographic()
{
	if(screenMode)	guOrtho(currentProjectionMtx, 0, 479, -104, 743, 0, 700);
	else			guOrtho(currentProjectionMtx, 0, 479, 0, 639, 0, 700);
	GX_LoadProjectionMtx(currentProjectionMtx, GX_ORTHOGRAPHIC);
}

void Graphics::setDepth(float newDepth)
{
	depth = newDepth;
}

float Graphics::getDepth()
{
	return depth;
}

void Graphics::setColor(GXColor color)
{
	for (int i = 0; i < 4; i++){
		currentColor[i].r = color.r;
		currentColor[i].g = color.g;
		currentColor[i].b = color.b;
		currentColor[i].a = color.a;
	}
	applyCurrentColor();
}

void Graphics::setColor(GXColor* color)
{
	for (int i = 0; i < 4; i++){
		currentColor[i].r = color[i].r;
		currentColor[i].g = color[i].g;
		currentColor[i].b = color[i].b;
		currentColor[i].a = color[i].a;
	}
	applyCurrentColor();
}

void Graphics::drawRect(int x, int y, int width, int height)
{
	GX_Begin(GX_LINESTRIP, GX_VTXFMT0, 4);
		GX_Position3f32((float) x,(float) y, depth );
		GX_Color4u8(appliedColor[0].r, appliedColor[0].g, appliedColor[0].b, appliedColor[0].a);
		GX_TexCoord2f32(0.0f,0.0f);
		GX_Position3f32((float) (x+width),(float) y, depth );
		GX_Color4u8(appliedColor[1].r, appliedColor[1].g, appliedColor[1].b, appliedColor[1].a);
		GX_TexCoord2f32(0.0f,0.0f);
		GX_Position3f32((float) (x+width),(float) (y+height), depth );
		GX_Color4u8(appliedColor[2].r, appliedColor[2].g, appliedColor[2].b, appliedColor[2].a);
		GX_TexCoord2f32(0.0f,0.0f);
		GX_Position3f32((float) x,(float) (y+height), depth );
		GX_Color4u8(appliedColor[3].r, appliedColor[3].g, appliedColor[3].b, appliedColor[3].a);
		GX_TexCoord2f32(0.0f,0.0f);
	GX_End();
}

void Graphics::fillRect(int x, int y, int width, int height)
{
	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
		GX_Position3f32((float) x,(float) y, depth );
		GX_Color4u8(appliedColor[0].r, appliedColor[0].g, appliedColor[0].b, appliedColor[0].a);
		GX_TexCoord2f32(0.0f,0.0f);
		GX_Position3f32((float) (x+width),(float) y, depth );
		GX_Color4u8(appliedColor[1].r, appliedColor[1].g, appliedColor[1].b, appliedColor[1].a);
		GX_TexCoord2f32(0.0f,0.0f);
		GX_Position3f32((float) (x+width),(float) (y+height), depth );
		GX_Color4u8(appliedColor[2].r, appliedColor[2].g, appliedColor[2].b, appliedColor[2].a);
		GX_TexCoord2f32(0.0f,0.0f);
		GX_Position3f32((float) x,(float) (y+height), depth );
		GX_Color4u8(appliedColor[3].r, appliedColor[3].g, appliedColor[3].b, appliedColor[3].a);
		GX_TexCoord2f32(0.0f,0.0f);
	GX_End();
}

void Graphics::drawImage(int textureId, int x, int y, int width, int height, float s1, float s2, float t1, float t2)
{
	//Init texture here or in calling code?
	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
		GX_Position3f32((float) x,(float) y, depth );
		GX_Color4u8(appliedColor[0].r, appliedColor[0].g, appliedColor[0].b, appliedColor[0].a);
		GX_TexCoord2f32(s1,t1);
		GX_Position3f32((float) (x+width),(float) y, depth );
		GX_Color4u8(appliedColor[1].r, appliedColor[1].g, appliedColor[1].b, appliedColor[1].a);
		GX_TexCoord2f32(s2,t1);
		GX_Position3f32((float) (x+width),(float) (y+height), depth );
		GX_Color4u8(appliedColor[2].r, appliedColor[2].g, appliedColor[2].b, appliedColor[2].a);
		GX_TexCoord2f32(s2,t2);
		GX_Position3f32((float) x,(float) (y+height), depth );
		GX_Color4u8(appliedColor[3].r, appliedColor[3].g, appliedColor[3].b, appliedColor[3].a);
		GX_TexCoord2f32(s1,t2);
	GX_End();
}

void Graphics::drawLine(int x1, int y1, int x2, int y2)
{
	GX_Begin(GX_LINES, GX_VTXFMT0, 2);
		GX_Position3f32((float) x1,(float) y1, depth );
		GX_Color4u8(appliedColor[0].r, appliedColor[0].g, appliedColor[0].b, appliedColor[0].a);
		GX_TexCoord2f32(0.0f,0.0f);
		GX_Position3f32((float) x2,(float) y2, depth );
		GX_Color4u8(appliedColor[1].r, appliedColor[1].g, appliedColor[1].b, appliedColor[1].a);
		GX_TexCoord2f32(0.0f,0.0f);
	GX_End();
}

#ifndef PI
#define PI 3.14159f
#endif

void Graphics::drawCircle(int x, int y, int radius, int numSegments)
{

	GX_Begin(GX_LINESTRIP, GX_VTXFMT0, numSegments+1);

	for (int i = 0; i<=numSegments; i++)
	{
		float angle, point_x, point_y;
		angle = 2*PI * i/numSegments;
		point_x = (float)x + (float)radius * cos( angle );
		point_y = (float)y + (float)radius * sin( angle );

		GX_Position3f32((float) point_x,(float) point_y, depth );
		GX_Color4u8(appliedColor[0].r, appliedColor[0].g, appliedColor[0].b, appliedColor[0].a);
		GX_TexCoord2f32(0.0f,0.0f);
	}

	GX_End();
}

void Graphics::drawString(int x, int y, const std::string &str)
{
	//todo
}

void Graphics::drawPoint(int x, int y, int radius)
{
	GX_SetPointSize(u8 (radius *3 ),GX_TO_ZERO);
	GX_Begin(GX_POINTS, GX_VTXFMT0, 1);
		GX_Position3f32((float) x,(float) y, depth );
		GX_Color4u8(appliedColor[0].r, appliedColor[0].g, appliedColor[0].b, appliedColor[0].a);
		GX_TexCoord2f32(0.0f,0.0f);
	GX_End();
}

void Graphics::setLineWidth(int width)
{
	GX_SetLineWidth((u8) (width * 6), GX_TO_ZERO );
}

void Graphics::pushDepth(float d)
{
	depthStack.push(getDepth());
	setDepth(d);
}

void Graphics::popDepth()
{
	depthStack.pop();
	if(depthStack.size() != 0)
	{
		setDepth(depthStack.top());
	}
	else
	{
		setDepth(1.0f);
	}
}

void Graphics::enableScissor(int x, int y, int width, int height)
{
	if(screenMode)
	{
		int x1 = (x+104)*640/848;
		int x2 = (x+width+104)*640/848;
		GX_SetScissor((u32) x1,(u32) y,(u32) x2-x1,(u32) height);
	}
	else
		GX_SetScissor((u32) x,(u32) y,(u32) width,(u32) height);
}

void Graphics::disableScissor()
{
	GX_SetScissor((u32) 0,(u32) 0,(u32) viewportWidth,(u32) viewportHeight); //Set to the same size as the viewport.
}

void Graphics::enableBlending(bool blend)
{
	if (blend)
		GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
	else
		GX_SetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR);
}

void Graphics::setTEV(int tev_op)
{
	GX_SetNumTevStages(1);
	GX_SetNumChans (1);
	GX_SetNumTexGens (1);
	GX_SetTevOrder (GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
	GX_SetTevOp (GX_TEVSTAGE0, tev_op);
	GX_SetTevSwapModeTable(GX_TEV_SWAP0, GX_CH_RED, GX_CH_GREEN, GX_CH_BLUE, GX_CH_ALPHA);
	GX_SetTevSwapMode(GX_TEVSTAGE0, GX_TEV_SWAP0, GX_TEV_SWAP0);
}

void Graphics::pushTransparency(float f)
{
	transparencyStack.push(getTransparency());
	setTransparency(f);
}

void Graphics::popTransparency()
{
	transparencyStack.pop();
	if(transparencyStack.size() != 0)
	{
		setTransparency(transparencyStack.top());
	}
	else
	{
		setTransparency(1.0f);
	}
}

void Graphics::setTransparency(float f)
{
	transparency = f;
	applyCurrentColor();
}

float Graphics::getTransparency()
{
	return transparency;
}

void Graphics::applyCurrentColor()
{
	for (int i = 0; i < 4; i++){
		appliedColor[i].r = currentColor[i].r;
		appliedColor[i].g = currentColor[i].g;
		appliedColor[i].b = currentColor[i].b;
		appliedColor[i].a = (u8) (getCurrentTransparency(i) * 255.0f);
	}
}

float Graphics::getCurrentTransparency(int index)
{
	float alpha = (float)currentColor[index].a/255.0f;
	float val = alpha * transparency;
	return val;
}

} //namespace menu
