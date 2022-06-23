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
#include "../wiiSXconfig.h"

namespace menu {

#define CH_FONT_HEIGHT 24
#define CH_FONT_WIDTH 24

#define CHAR_IMG_SIZE 1152

static std::map<wchar_t, int> charCodeMap;
static std::map<wchar_t, u8*> charPngBufMap;
heap_cntrl* GXtexCache;

IplFont::IplFont()
		: frameWidth(640)
{
	//int ZhBufFont_size = 8060788; //8032896; //8060788; // RGB565, IA8
	//int searchLen = (int)(ZhBufFont_size / (CHAR_IMG_SIZE + 4));

	int bufIndex = 0;
	int skipSetp = (CHAR_IMG_SIZE + 4) / 2;
	FILE* charPngFile;
	switch(lang)
    {
        case SIMP_CHINESE:
            charPngFile = fopen("sd:/wiisxrx/fonts/Chs.dat", "rb"); // 24 * 24 IA8
            break;

        default:
            charPngFile = fopen("sd:/wiisxrx/fonts/Chs.dat", "rb");
            break;
    }
	fseek(charPngFile, 0, SEEK_END);
	int fontSize = (int)ftell(charPngFile);
	int searchLen = (int)(fontSize / (CHAR_IMG_SIZE + 4));

	GXtexCache = (heap_cntrl*)malloc(sizeof(heap_cntrl));
	__lwp_heap_init(GXtexCache, CN_FONT_LO, CN_FONT_SIZE, 32);
	u8 *fontBuffer = (u8*) __lwp_heap_allocate(GXtexCache, fontSize);

	fseek(charPngFile, 0, SEEK_SET);
	fread(fontBuffer, 1, fontSize, charPngFile);
    fclose(charPngFile);
    charPngFile = NULL;

    blankChar = charToWideChar(" ");

	uint16_t *zhFontBufTemp = (uint16_t *)fontBuffer;
    while (bufIndex < searchLen)
    {
        charCodeMap.insert(std::pair<wchar_t, int>(*zhFontBufTemp, *((u8*)(zhFontBufTemp + 1) + 1)));
        u8 * tmpPngBuf = (u8*) __lwp_heap_allocate(GXtexCache, CHAR_IMG_SIZE);
        memcpy(tmpPngBuf, (u8*)(zhFontBufTemp + 2), CHAR_IMG_SIZE);
        charPngBufMap.insert(std::pair<wchar_t, u8*>(*zhFontBufTemp, tmpPngBuf));

        zhFontBufTemp += skipSetp;
        bufIndex++;
    }
    __lwp_heap_free(GXtexCache, fontBuffer);
}

IplFont::~IplFont()
{
    charCodeMap.clear();
    if (charPngBufMap.size() > 0)
    {
         std::map<wchar_t, u8*>::iterator it = charPngBufMap.begin();
         while (it != charPngBufMap.end())
         {
             u8* tmpBuf = it->second;
             __lwp_heap_free(GXtexCache, tmpBuf);
             ++it;
         }

         charPngBufMap.clear();
    }
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

	//GX_InvalidateTexAll();
	//GX_InitTexObj(&fontTexObj, ZhBufFont_dat, fontPngWidth, CH_FONT_HEIGHT, GX_TF_IA8, GX_CLAMP, GX_CLAMP, GX_FALSE);
	//GX_InitTexObj(&fontTexObj, ZhBufFont_dat, CH_FONT_WIDTH, fontPngHeight, GX_TF_IA8, GX_CLAMP, GX_CLAMP, GX_FALSE);
	//GX_LoadTexObj(&fontTexObj, GX_TEXMAP0);

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

__inline wchar_t* IplFont::charToWideChar(char* strChar) {
	wchar_t *strWChar = new wchar_t[strlen(strChar) + 1];

	int bt = mbstowcs(strWChar, strChar, strlen(strChar));
	if (bt) {
		strWChar[bt] = (wchar_t)'\0';
		return strWChar;
	}

	wchar_t *tempDest = strWChar;
	while((*tempDest++ = *strChar++));

	return strWChar;
}

__inline wchar_t* IplFont::charToWideChar(const char* strChar) {
	return charToWideChar((char*)strChar);
}

__inline u8* IplFont::getCharPngBuf(const wchar_t wChar) {
    std::map<wchar_t, u8*>::iterator iter = charPngBufMap.find(wChar);
	if (iter != charPngBufMap.end())
	{
	    return (u8*)(charPngBufMap[wChar]);
	}
    else
	{
	    return (u8*)(charPngBufMap[blankChar[0]]);
	}
}

__inline int IplFont::getCharCode(const wchar_t wChar) {
    std::map<wchar_t, int>::iterator iter = charCodeMap.find(wChar);
	if (iter != charCodeMap.end())
	{
	    return charCodeMap[wChar];
	}
    else
	{
	    return charCodeMap[blankChar[0]];
	}
}

void IplFont::drawString(int x, int y, char *string, float scale, bool centered)
{
	if(centered)
	{
		int strHeight = this->getStringHeight(string, scale);
		int strWidth = this->getStringWidth(string, scale);

		x = (int) x - strWidth/2;
		y = (int) y - strHeight/2;
	}

	GX_InvalidateTexAll();

    wchar_t *utf8Txt = charToWideChar(gettext(string));
    wchar_t *tmpPtr = utf8Txt;
	while (*utf8Txt) {

        GX_InitTexObj(&fontTexObj, this->getCharPngBuf(*utf8Txt), CH_FONT_WIDTH, CH_FONT_HEIGHT, GX_TF_IA8, GX_CLAMP, GX_CLAMP, GX_FALSE);
        GX_LoadTexObj(&fontTexObj, GX_TEXMAP0);

        GX_Begin(GX_QUADS, GX_VTXFMT1, 4);
            GX_Position2s16(x, y);
			GX_Color4u8(fontColor.r, fontColor.g, fontColor.b, fontColor.a);
			GX_TexCoord2f32(0.0f, 0.0f);

			GX_Position2s16(24 + x, y);
			GX_Color4u8(fontColor.r, fontColor.g, fontColor.b, fontColor.a);
			GX_TexCoord2f32(1.0f, 0.0f);

			GX_Position2s16(24 + x, 24 + y);
			GX_Color4u8(fontColor.r, fontColor.g, fontColor.b, fontColor.a);
			GX_TexCoord2f32(1.0f, 1.0f);

			GX_Position2s16(x, 24 + y);
			GX_Color4u8(fontColor.r, fontColor.g, fontColor.b, fontColor.a);
			GX_TexCoord2f32(0.0f, 1.0f);

        GX_End();

		x += this->getCharCode(*utf8Txt) + 1; // x + charWidth
		utf8Txt++;
	}

	free(tmpPtr);
}

int IplFont::drawStringWrap(int x, int y, char *string, float scale, bool centered, int maxWidth, int lineSpacing)
{
	int numLines = 0;
	int stringWidth = 0;
	int tokenWidth = 0;
	int numTokens = 0;
	char* utf8Txt = (char *)gettext(string);
	char* lineStart = utf8Txt;
	char* lineStop = utf8Txt;
	char* stringWork = utf8Txt;
	char* stringDraw = NULL;
	lineSpacing = CH_FONT_HEIGHT;

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
		tokenWidth += (int) CH_FONT_WIDTH * scale;

		stringWork++;
	}

	return numLines;
}

void IplFont::drawStringAtOrigin(char *string, float scale)
{
	int x0, y0, x = 0;
	wchar_t *utf8Txt = charToWideChar(gettext(string));
	wchar_t *tmpPtr = utf8Txt;
	while (*utf8Txt)
	{
		x += this->getCharCode(*utf8Txt);
		utf8Txt++;
	}
    x0 = (int) -x / 2;
	y0 = (int) -CH_FONT_HEIGHT / 2;
	free(tmpPtr);

	drawString(x0, y0, string, scale, false);
}

int IplFont::getStringWidth(char *string, float scale)
{
	int strWidth = 0;
	wchar_t *utf8Txt = charToWideChar(gettext(string));
	wchar_t *tmpPtr = utf8Txt;
	while(*utf8Txt)
	{
		strWidth += this->getCharCode(*utf8Txt);
		utf8Txt++;
	}
	free(tmpPtr);

	return strWidth;
}

int IplFont::getStringHeight(char *string, float scale)
{
	return CH_FONT_HEIGHT;
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
