/**
 * Wii64 - IPLFont.cpp
 * Copyright (C) 2009 sepp256
 *
 * Wii64 homepage: http://www.emulatemii.com
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

#include <map>
#include <ogc/lwp_heap.h>

#include "IPLFont.h"
#include "../MEM2.h"

#include "gui2/gettext.h"
#include "gui2/FreeTypeGX.h"

namespace menu {

#define STRHEIGHT_OFFSET 6
#define CH_FONT_HEIGHT 15

#define CH_FONT_SIZE 24

heap_cntrl* GXtexCache;
u8 *ttfFileBuf;
FreeTypeGX *fontSystemOne;

IplFont::IplFont()
		: frameWidth(640)
{
	GXtexCache = (heap_cntrl*)malloc(sizeof(heap_cntrl));
	__lwp_heap_init(GXtexCache, FONTWORK_HI, FONTWORK_SIZE, 32);

	FILE* ttfFile = fopen("sd:/wiisxr/fonts/font_zh.ttf", "r");
	fseek(ttfFile, 0, SEEK_END);	// Calculate the size of the font by seeking to the end of the file
	int fileSize = ftell(ttfFile);	// ...and getting your file cursor position.
	rewind(ttfFile);	// Back up to the head of the file.

	u8* ttfFileBuf = (u8*) __lwp_heap_allocate(GXtexCache, fileSize); // Allocate your font buffer
	fread (ttfFileBuf, 1, fileSize, ttfFile);	// ...and read the font data into the buffer

	if (ttfFile != NULL) {
		fclose(ttfFile);	// Be kind. Always close files that you have opened.
	}

    fontSystemOne = new FreeTypeGX(GX_TF_IA8);
    fontSystemOne->loadFont(ttfFileBuf, fileSize, CH_FONT_SIZE, false);
}

IplFont::~IplFont()
{
	if (fontSystemOne)
    {
        delete fontSystemOne;
    }

    __lwp_heap_free(GXtexCache, ttfFileBuf);
}

void IplFont::setVmode(GXRModeObj *rmode)
{
	vmode = rmode;
}

extern "C" char menuActive;

void IplFont::drawInit(GXColor fontColor)
{
    setColor(fontColor);

    //FixMe: vmode access
	Mtx44 GXprojection2D;
	Mtx GXmodelView2D;

	// Reset various parameters from gfx plugin
	GX_SetCoPlanar(GX_DISABLE);
	GX_SetClipMode(GX_CLIP_ENABLE);
//	GX_SetScissor(0,0,vmode->fbWidth,vmode->efbHeight);
	GX_SetAlphaCompare(GX_ALWAYS,0,GX_AOP_AND,GX_ALWAYS,0);

	guMtxIdentity(GXmodelView2D);
	GX_LoadTexMtxImm(GXmodelView2D,GX_TEXMTX0,GX_MTX2x4);
//	guMtxTransApply (GXmodelView2D, GXmodelView2D, 0.0F, 0.0F, -5.0F);
	GX_LoadPosMtxImm(GXmodelView2D,GX_PNMTX0);
	if(screenMode && menuActive)
		guOrtho(GXprojection2D, 0, 479, -104, 743, 0, 700);
	else if(screenMode == SCREENMODE_16x9_PILLARBOX)
		guOrtho(GXprojection2D, 0, 479, -104, 743, 0, 700);
	else
		guOrtho(GXprojection2D, 0, 479, 0, 639, 0, 700);
	GX_LoadProjectionMtx(GXprojection2D, GX_ORTHOGRAPHIC);
//	GX_SetViewport (0, 0, vmode->fbWidth, vmode->efbHeight, 0, 1);

	GX_SetZMode(GX_DISABLE,GX_ALWAYS,GX_TRUE);

	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_PTNMTXIDX, GX_PNMTX0);
	GX_SetVtxDesc(GX_VA_TEX0MTXIDX, GX_TEXMTX0);
//	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
//	GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
//	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
//	GX_SetVtxAttrFmt(GX_VTXFMT1, GX_VA_POS, GX_POS_XYZ, GX_S16, 0);
//	GX_SetVtxAttrFmt(GX_VTXFMT1, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
//	GX_SetVtxAttrFmt(GX_VTXFMT1, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

    GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
    GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
	//set vertex attribute formats here
	GX_SetVtxAttrFmt(GX_VTXFMT1, GX_VA_POS, GX_POS_XY, GX_S16, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT1, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
//	GX_SetVtxAttrFmt(GX_VTXFMT1, GX_VA_TEX0, GX_TEX_ST, GX_U16, 7);
	GX_SetVtxAttrFmt(GX_VTXFMT1, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

	//enable textures
	GX_SetNumChans (1);
//	GX_SetChanCtrl(GX_COLOR0A0,GX_DISABLE,GX_SRC_REG,GX_SRC_VTX,GX_LIGHTNULL,GX_DF_NONE,GX_AF_NONE);
	GX_SetNumTexGens (1);
	GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);

    GX_SetNumTevStages (1);
    GX_SetTevOrder (GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0); // change to (u8) tile later
    GX_SetTevColorIn (GX_TEVSTAGE0, GX_CC_C1, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO);
    GX_SetTevColorOp (GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_ENABLE, GX_TEVPREV);
    GX_SetTevAlphaIn (GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_A1, GX_CA_TEXA, GX_CA_ZERO);
    GX_SetTevAlphaOp (GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_ENABLE, GX_TEVPREV);

	//set blend mode
	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR); //Fix src alpha
	GX_SetColorUpdate(GX_ENABLE);
//	GX_SetAlphaUpdate(GX_ENABLE);
//	GX_SetDstAlpha(GX_DISABLE, 0xFF);
	//set cull mode
	GX_SetCullMode (GX_CULL_NONE);

}

void IplFont::setColor(GXColor fontColour)
{
	GX_SetTevColor(GX_TEVREG1, fontColour);
//	GX_SetTevKColor(GX_KCOLOR0, fontColour);
	fontColor.r = fontColour.r;
	fontColor.g = fontColour.g;
	fontColor.b = fontColour.b;
	fontColor.a = fontColour.a;
}

void IplFont::setColor(GXColor* fontColorPtr)
{
	GX_SetTevColor(GX_TEVREG1, *fontColorPtr);
//	GX_SetTevKColor(GX_KCOLOR0, *fontColorPtr);
	fontColor.r = fontColorPtr->r;
	fontColor.g = fontColorPtr->g;
	fontColor.b = fontColorPtr->b;
	fontColor.a = fontColorPtr->a;
}

static char * ChgCoustString(const char *s)
{
    char *result = (char *)malloc(strlen(s) + 1);
    if (result == NULL)
    {
        return (char *)s;
    }

    strcpy(result, s);

    return result;
}

void IplFont::drawString(int x, int y, char *string, float scale, bool centered)
{
	char *utf8Txt = ChgCoustString(gettext(string));
    wchar_t *text = fontSystemOne->charToWideChar(utf8Txt);

	if(centered)
	{
		int strHeight = (fontSystemOne->getHeight(text) + STRHEIGHT_OFFSET) * scale;
		int strWidth = fontSystemOne->getWidth(text);

		x = (int) x - strWidth/2;
		y = (int) y - strHeight/2;
	}

	fontSystemOne->drawText(x, y + CH_FONT_HEIGHT, text, fontColor, (u16)(FTGX_JUSTIFY_MASK | FTGX_ALIGN_MIDDLE));
}

int IplFont::drawStringWrap(int x, int y, char *string, float scale, bool centered, int maxWidth, int lineSpacing)
{
	int numLines = 0;
	int stringWidth = 0;
	int tokenWidth = 0;
	int numTokens = 0;
	char *utf8Txt = ChgCoustString(gettext(string));
	//char* lineStart = (char *)gettext(string);
	//char* lineStop = (char *)gettext(string);
	//char* stringWork = (char *)gettext(string);
	char* lineStart = utf8Txt;
	char* lineStop = utf8Txt;
	char* stringWork = utf8Txt;
	char* stringDraw = NULL;

	while(1)
	{
		if(*stringWork == 0) //end of string
		{
			if((stringWidth + tokenWidth <= maxWidth) || (numTokens = 0))
			{
				if (stringWidth + tokenWidth > 0)
				{
					drawString( x, y+numLines*lineSpacing, lineStart, scale, centered);
					numLines++;
				}
				break;
			}
			else
			{
				stringDraw = (char*)malloc(lineStop - lineStart + 1);
				for (int i = 0; i < lineStop-lineStart; i++)
					stringDraw[i] = lineStart[i];
				stringDraw[lineStop-lineStart] = 0;
				drawString( x, y+numLines*lineSpacing, stringDraw, scale, centered);
				free(stringDraw);
				numLines++;
				lineStart = lineStop+1;
				drawString( x, y+numLines*lineSpacing, lineStart, scale, centered);
				numLines++;
				break;
			}
		}

		if((*stringWork == ' ')) //end of token
		{
			if(stringWidth + tokenWidth <= maxWidth)
			{
				stringWidth += tokenWidth;
				numTokens++;
				tokenWidth = 0;
				lineStop = stringWork;
			}
			else
			{
				if (numTokens == 0)	//if the word is wider than maxWidth, just print it
					lineStop = stringWork;

				stringDraw = (char*)malloc(lineStop - lineStart + 1);
				for (int i = 0; i < lineStop-lineStart; i++)
					stringDraw[i] = lineStart[i];
				stringDraw[lineStop-lineStart] = 0;
				drawString( x, y+numLines*lineSpacing, stringDraw, scale, centered);
				free(stringDraw);
				numLines++;

				lineStart = lineStop+1;
				lineStop = lineStart;
				stringWork = lineStart;
				stringWidth = 0;
				tokenWidth = 0;
				numTokens = 0;
				continue;
			}
		}
		//tokenWidth += (int) fontChars.font_size[(int)(*stringWork)] * scale;
		tokenWidth += fontSystemOne->getWidth(fontSystemOne->charToWideChar(stringWork)) * scale;

		stringWork++;
	}

	return numLines;
}

void IplFont::drawStringAtOrigin(char *string, float scale)
{
	int x0, y0, x = 0;
	char *utf8Txt = ChgCoustString(gettext(string));
    wchar_t *text = fontSystemOne->charToWideChar(utf8Txt);
    x = fontSystemOne->getWidth(text);

	x0 = (int) -x/2;
	y0 = (int) -(fontSystemOne->getHeight(text) + STRHEIGHT_OFFSET)*scale/2;
	drawString(x0, y0, string, scale, false);
}

int IplFont::getStringWidth(char *string, float scale)
{
	char *utf8Txt = ChgCoustString(gettext(string));
    wchar_t *text = fontSystemOne->charToWideChar(utf8Txt);
    int strWidth = fontSystemOne->getWidth(text) * scale;

	return strWidth;
}

int IplFont::getStringHeight(char *string, float scale)
{
	char *utf8Txt = ChgCoustString(gettext(string));
    wchar_t *text = fontSystemOne->charToWideChar(utf8Txt);
	int strHeight = (fontSystemOne->getHeight(text) + STRHEIGHT_OFFSET) * scale;

	return strHeight;
}

} //namespace menu

extern "C" {
void IplFont_drawInit(GXColor fontColor)
{
	menu::IplFont::getInstance().drawInit(fontColor);
}

void IplFont_drawString(int x, int y, char *string, float scale, bool centered)
{
	menu::IplFont::getInstance().drawString(x, y, string, scale, centered);
}
}
