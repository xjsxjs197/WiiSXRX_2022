/**
 * Wii64 - InputManager.cpp
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
#include <ogc/machine/processor.h>

#include "InputManager.h"
#include "FocusManager.h"
#include "CursorManager.h"
#include "../gc_input/controller.h"
#include "../DEBUG.h"

#include "../../HidController/KernelHID.h"
bool isWiiVC = false;

void ShutdownWii();
void initHid();



namespace menu {

Input::Input()
{
#ifdef HW_RVL
	initHid();
#endif // HW_RVL

	PAD_Init();
#ifdef HW_RVL
	CONF_Init();
	WUPC_Init();
	//WiiDRC_Init();
	//isWiiVC = WiiDRC_Inited();
	WPAD_Init();
	WPAD_SetIdleTimeout(120);
	WPAD_SetVRes(WPAD_CHAN_ALL, 640, 480);
	WPAD_SetDataFormat(WPAD_CHAN_ALL, WPAD_FMT_BTNS_ACC_IR);
	WPAD_SetPowerButtonCallback((WPADShutdownCallback)ShutdownWii);
	SYS_SetPowerCallback(ShutdownWii);

	/* HID */
	HIDUpdateControllerIni();

#endif
//	VIDEO_SetPostRetraceCallback (PAD_ScanPads);
}

Input::~Input()
{
#ifdef HW_RVL
	WUPC_Shutdown();
	WPAD_Shutdown();
#endif
}

void Input::initHid()
{
    int i;
    WiiDRC_Init();
    isWiiVC = WiiDRC_Inited();
	//Set some important kernel regs
//	*(vu32*)0x92FFFFC0 = isWiiVC; //cant be detected in IOS
//	if(WiiDRC_Connected()) //used in PADReadGC.c
//		*(vu32*)0x92FFFFC4 = (u32)WiiDRC_GetRawI2CAddr();
//	else //will disable gamepad spot for player 1
//		*(vu32*)0x92FFFFC4 = 0;
//	DCFlushRange((void*)0x92FFFFC0,0x20);

	//memset((void*)0x93003010, 0, 0x190); //clears alot of pad stuff
	//DCFlushRange((void*)0x93003010, 0x190);
	struct BTPadCont *BTPad = (struct BTPadCont*)0x932F0000;
	for(i = 0; i < WPAD_MAX_WIIMOTES; ++i)
		BTPad[i].used = C_NOT_SET;
}

void Input::refreshInput()
{
	if(padNeedScan){ gc_connected = PAD_ScanPads(); padNeedScan = 0; }
	PAD_Read(gcPad);
	PAD_Clamp(gcPad);
#ifdef HW_RVL
	if (wpadNeedScan){ WUPC_UpdateButtonStats(); WiiDRC_ScanPads(); WPAD_ScanPads(); wpadNeedScan = 0; }
//	WPAD_ScanPads();
	wiiPad = WPAD_Data(0);
	wupcData = WUPC_Data(0);
	wiidrcData = WiiDRC_Data();

	if (hidPadNeedScan)
	{
		#ifdef DISP_DEBUG
        sprintf(txtbuffer, "NeedScan hidConnected %d\r\n", hidControllerConnected);
        writeLogFile(txtbuffer);
        #endif // DISP_DEBUG
		hidPadNeedScan = 0;
		if (hidControllerConnected)
		{
			HIDUpdateControllerIni();
		    HIDReadData();
		    #ifdef DISP_DEBUG
		    //PADStatusKernel *Pad = (PADStatusKernel*)(0x93003100); //PadBuff
            //sprintf(txtbuffer, "BTN %08x\r\n", Pad[0].button);
            //writeLogFile(txtbuffer);
            #endif // DISP_DEBUG
		}
	}
#endif
}

#ifdef HW_RVL
WPADData* Input::getWpad()
{
	return wiiPad;
}

WUPCData* Input::getWupc()
{
	return wupcData;
}

const WiiDRCData* Input::getWiiDRC()
{
	return wiidrcData;
}
#endif

PADStatus* Input::getPad()
{
	return &gcPad[0];
}

void Input::clearInputData()
{
	Focus::getInstance().clearInputData();
	Cursor::getInstance().clearInputData();
}

} //namespace menu
