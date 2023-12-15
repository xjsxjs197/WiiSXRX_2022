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
#include <unistd.h>
#include <fatfs/diskio.h>
#include <fatfs/ff.h>
#include <fatfs/ff_utf8.h>

#include "InputManager.h"
#include "FocusManager.h"
#include "CursorManager.h"
#include "../gc_input/controller.h"
#include "PADReadGC_bin.h"
#include "kernel_bin.h"
#include "kernelboot_bin.h"

extern "C" {
    #include "../../Nintendont/HID.h"
    #include "../../Nintendont/Patches.h"
    #include "../../Nintendont/global.h"
}

bool isWiiVC = false;

void ShutdownWii();
void initHid();

static ioctlv IOCTL_Buf[2] ALIGNED(32);
static u32 (*const PADRead)(u32) = (void*)0x93000000;
#define STATUS			((void*)0x90004100)
#define STATUS_LOADING	(*(volatile unsigned int*)(0x90004100))
#define MEM_PROT		0xD8B420A
#define C_NOT_SET	(0<<0)
#define WPAD_MAX_WIIMOTES 4

static char dev_es[] ATTRIBUTE_ALIGN(32) = "/dev/es";
struct BTPadCont {
	u32 used;
	s16 xAxisL;
	s16 xAxisR;
	s16 yAxisL;
	s16 yAxisR;
	u32 button;
	u8 triggerL;
	u8 triggerR;
	s16 xAccel;
	s16 yAccel;
	s16 zAccel;
} __attribute__((aligned(32)));

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

	HIDUpdateRegisters();
	//writeLogFile("HIDUpdateRegisters==OK==\r\n");
	PADRead(0);
	hidGcPad = (PADStatus*)(0x93003100); //PadBuff
	//writeLogFile("PADRead==OK==\r\n");

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
	//writeLogFile("initHid==11111==\r\n");

	// inject nintendont kernel
	memcpy((void*)0x92F00000,kernel_bin, kernel_bin_size);
	DCFlushRange((void*)0x92F00000, kernel_bin_size);

	// inject kernelboot
	memcpy((void*)0x92FFFE00,kernelboot_bin, kernelboot_bin_size);
	DCFlushRange((void*)0x92FFFE00,kernelboot_bin_size);

	//close in case this is wii vc
	__ES_Close();
	memset( STATUS, 0, 0x20 );
	DCFlushRange( STATUS, 0x20 );
	//make sure kernel doesnt reload
	*(vu32*)0x93003420 = 0;
	DCFlushRange((void*)0x93003420,0x20);

	//Set some important kernel regs
	*(vu32*)0x92FFFFC0 = isWiiVC; //cant be detected in IOS
	if(WiiDRC_Connected()) //used in PADReadGC.c
		*(vu32*)0x92FFFFC4 = (u32)WiiDRC_GetRawI2CAddr();
	else //will disable gamepad spot for player 1
		*(vu32*)0x92FFFFC4 = 0;
	DCFlushRange((void*)0x92FFFFC0,0x20);

	//writeLogFile("initHid==2222222==\r\n");
	s32 fd;
	fd = IOS_Open( dev_es, 0 );
	IOS_IoctlvAsync(fd, 0x1F, 0, 0, IOCTL_Buf, NULL, NULL);
	//Waiting for Nintendont...
	//writeLogFile("initHid==3333333333==\r\n");
	while(1)
	{
		DCInvalidateRange( STATUS, 0x20 );
		if((STATUS_LOADING > 0 || STATUS_LOADING > 1 || STATUS_LOADING < -1) && STATUS_LOADING < 20)
		{
			//printf("Kernel sent signal\n");
			break;
		}
	}
	//Async Ioctlv done by now
	//writeLogFile("initHid==444444444==\r\n");
	IOS_Close(fd);
	//writeLogFile("initHid==5555555==\r\n");

	// Initialize controllers.
	DCInvalidateRange((void*)0x93000000, 0x3000);
	memcpy((void*)0x93000000, PADReadGC_bin, PADReadGC_bin_size);
	DCFlushRange((void*)0x93000000, 0x3000);
	ICInvalidateRange((void*)0x93000000, 0x3000);
	DCInvalidateRange((void*)0x93003010, 0x190);
	memset((void*)0x93003010, 0, 0x190); //clears alot of pad stuff
	DCFlushRange((void*)0x93003010, 0x190);
	struct BTPadCont *BTPad = (struct BTPadCont*)0x932F0000;
	int i;
	for(i = 0; i < WPAD_MAX_WIIMOTES; ++i)
		BTPad[i].used = C_NOT_SET;
	//writeLogFile("initHid==OK==\r\n");
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
	    static volatile hidController *HID_CTRL = (volatile hidController*)0x93005000;
		hidGcConnected = ((HID_CTRL->VID != 0 && HID_CTRL->PID != 0) ? 1 : 0);
	    hidPadNeedScan = 0;
	}
	HIDUpdateRegisters();
	PADRead(0);
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

PADStatus* Input::getHidPad()
{
    return hidGcPad;
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
