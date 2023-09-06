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

static unsigned char ESBootPatch[] =
{
    0x48, 0x03, 0x49, 0x04, 0x47, 0x78, 0x46, 0xC0, 0xE6, 0x00, 0x08, 0x70, 0xE1, 0x2F, 0xFF, 0x1E,
    0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x0D, 0x25,
};
/*static const unsigned char AHBAccessPattern[] =
{
	0x68, 0x5B, 0x22, 0xEC, 0x00, 0x52, 0x18, 0x9B, 0x68, 0x1B, 0x46, 0x98, 0x07, 0xDB,
};
static const unsigned char AHBAccessPatch[] =
{
	0x68, 0x5B, 0x22, 0xEC, 0x00, 0x52, 0x18, 0x9B, 0x23, 0x01, 0x46, 0x98, 0x07, 0xDB,
};*/
static const unsigned char FSAccessPattern[] =
{
    0x9B, 0x05, 0x40, 0x03, 0x99, 0x05, 0x42, 0x8B,
};
static const unsigned char FSAccessPatch[] =
{
    0x9B, 0x05, 0x40, 0x03, 0x1C, 0x0B, 0x42, 0x8B,
};

static char dev_es[] ATTRIBUTE_ALIGN(32) = "/dev/es";

extern vu32 FoundVersion;

namespace menu {

Input::Input()
{

	PAD_Init();
#ifdef HW_RVL
    RAMInit();

	CONF_Init();
	WiiDRC_Init();
	isWiiVC = WiiDRC_Inited();

	*(vu32*)0x92FFFFC0 = isWiiVC; //cant be detected in IOS
	if(WiiDRC_Connected()) //used in PADReadGC.c
		*(vu32*)0x92FFFFC4 = (u32)WiiDRC_GetRawI2CAddr();
	else //will disable gamepad spot for player 1
		*(vu32*)0x92FFFFC4 = 0;
	DCFlushRange((void*)0x92FFFFC0,0x20);

	s32 fd;
	/* Wii VC fw.img is pre-patched but Wii/vWii isnt, so we
		still have to reload IOS on those with a patched kernel */
	if(!isWiiVC)
	{
	    u32 u;
		//Disables MEMPROT for patches
		write16(MEM_PROT, 0);
		//Patches FS access
//		for( u = 0x93A00000; u < 0x94000000; u+=2 )
//		{
//			if( memcmp( (void*)(u), FSAccessPattern, sizeof(FSAccessPattern) ) == 0 )
//			{
//				//gprintf("FSAccessPatch:%08X\r\n", u );
//				memcpy( (void*)u, FSAccessPatch, sizeof(FSAccessPatch) );
//				DCFlushRange((void*)u, sizeof(FSAccessPatch));
//				break;
//			}
//		}

		// Load and patch IOS58.
		if (LoadKernel() >= 0)
		{
		    PatchKernel(isWiiVC);
            u32 v = FoundVersion;
            //this ensures all IOS modules get loaded in ES on reload
            memcpy( ESBootPatch+0x14, &v, 4 );
            DCInvalidateRange( (void*)0x939F0348, sizeof(ESBootPatch) );
            memcpy( (void*)0x939F0348, ESBootPatch, sizeof(ESBootPatch) );
            DCFlushRange( (void*)0x939F0348, sizeof(ESBootPatch) );

            //libogc still has that, lets close it
            __ES_Close();
            fd = IOS_Open( dev_es, 0 );

            irq_handler_t irq_handler = BeforeIOSReload();
            IOS_IoctlvAsync( fd, 0x25, 0, 0, IOCTL_Buf, NULL, NULL );
            sleep(1); //wait this time at least
            AfterIOSReload( irq_handler, v );
            //Disables MEMPROT for patches
            write16(MEM_PROT, 0);
		}
	}

	initHid();

	HIDUpdateRegisters();
	PADRead(0);
	hidGcPad = (PADStatus*)(0x93003100); //PadBuff

	WUPC_Init();
	WPAD_Init();
	WPAD_SetIdleTimeout(120);
	WPAD_SetVRes(WPAD_CHAN_ALL, 640, 480);
	WPAD_SetDataFormat(WPAD_CHAN_ALL, WPAD_FMT_BTNS_ACC_IR);
	WPAD_SetPowerButtonCallback((WPADShutdownCallback)ShutdownWii);
	SYS_SetPowerCallback(ShutdownWii);

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

void initHid()
{
	// for BT.c
	CONF_GetPadDevices((conf_pads*)0x932C0000);
	DCFlushRange((void*)0x932C0000, sizeof(conf_pads));
	*(vu32*)0x932C0490 = CONF_GetIRSensitivity();
	*(vu32*)0x932C0494 = CONF_GetSensorBarPosition();
	DCFlushRange((void*)0x932C0490, 8);

	// inject nintendont kernel
	memcpy((void*)0x92F00000,kernel_bin, kernel_bin_size);
	DCFlushRange((void*)0x92F00000, kernel_bin_size);

	// inject kernelboot
	memcpy((void*)0x92FFFE00,kernelboot_bin, kernelboot_bin_size);
	DCFlushRange((void*)0x92FFFE00,kernelboot_bin_size);

	memset( STATUS, 0, 0x20 );
	DCFlushRange( STATUS, 0x20 );
	//make sure kernel doesnt reload
	*(vu32*)0x93003420 = 0;
	DCFlushRange((void*)0x93003420,0x20);

	static ioctlv IOCTL_Buf[2] __attribute__((aligned(32)));
	s32 fd;
	fd = IOS_Open( dev_es, 0 );
	IOS_IoctlvAsync(fd, 0x1F, 0, 0, IOCTL_Buf, NULL, NULL);
	//Waiting for Nintendont...
	while(1)
	{
		DCInvalidateRange( STATUS, 0x20 );
		if((STATUS_LOADING > 0 || STATUS_LOADING > 1 || STATUS_LOADING < -1) && STATUS_LOADING < 20)
		{
			printf("Kernel sent signal\n");
			break;
		}
	}
	//Async Ioctlv done by now
	IOS_Close(fd);

	// Initialize controllers.
	DCInvalidateRange((void*)0x93000000, 0x3000);
	memcpy((void*)0x93000000, PADReadGC_bin, PADReadGC_bin_size);
	DCFlushRange((void*)0x93000000, 0x3000);
	ICInvalidateRange((void*)0x93000000, 0x3000);
	DCInvalidateRange((void*)0x93003010, 0x190);
	memset((void*)0x93003010, 0, 0x190); //clears alot of pad stuff
	DCFlushRange((void*)0x93003010, 0x190);
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
	    static u32* HID_STATUS = (u32*)0xD3003440;
		hidGcConnected = ((*HID_STATUS == 0) ? 0 : 1);
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
