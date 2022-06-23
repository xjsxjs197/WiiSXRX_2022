/**
 * Wii64 - IPLFont.h
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

#ifndef IPLFONT_H
#define IPLFONT_H

#include "GuiTypes.h"

namespace menu {

class IplFont
{
public:
	void setVmode(GXRModeObj *rmode);
	void drawInit(GXColor fontColor);
	void setColor(GXColor fontColor);
	void setColor(GXColor* fontColorPtr);
	void drawString(int x, int y, char *string, float scale, bool centered);
	int drawStringWrap(int x, int y, char *string, float scale, bool centered, int maxWidth, int lineSpacing);
	void drawStringAtOrigin(char *string, float scale);
	int getStringWidth(char *string, float scale);
	int getStringHeight(char *string, float scale);
	static IplFont& getInstance()
	{
		static IplFont obj;
		return obj;
	}
	~IplFont();

private:
	IplFont();

    u8* getPngPosByCharCode(wchar_t unicode, bool copyToBuf = false);
    int getPngBufPtr(char *string);
    wchar_t* charToWideChar(char* strChar);
    wchar_t* charToWideChar(const char* strChar);

	u16 frameWidth;
	GXTexObj fontTexObj;
	GXRModeObj *vmode;
	GXColor fontColor;

    u8 *pngBufData;
    u8 *zhFontBuf;
    uint16_t * zhFontBufTemp;
};

} //namespace menu

#endif
