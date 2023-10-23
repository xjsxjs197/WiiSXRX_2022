/**
 * WiiSX - PadSSSPSX.c
 * Copyright (C) 2007, 2008, 2009 Mike Slegeir
 * Copyright (C) 2007, 2008, 2009, 2010 sepp256
 * Copyright (C) 2007, 2008, 2009 emu_kidid
 *
 * Basic Analog PAD plugin for WiiSX
 *
 * WiiSX homepage: http://www.emulatemii.com
 * email address: tehpola@gmail.com
 *                sepp256@gmail.com
 *                emukidid@gmail.com
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

#include <gccore.h>
#include <stdint.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <ogc/pad.h>
#include <wiiuse/wpad.h>
#include "../plugins.h"
#include "../psxcommon.h"
#include "../psemu_plugin_defs.h"
#include "gc_input/controller.h"
#include "wiiSXconfig.h"
#include "PadSSSPSX.h"

//static BUTTONS PAD_1;
//static BUTTONS PAD_2;
extern PadDataS lastport1;
extern PadDataS lastport2;
static int pad_initialized = 0;

static struct
{
	SSSConfig config;	//unused?
	//int devcnt;			//unused
	u16 padStat[10];		//Digital Buttons
	int padID[10];
	int padMode1[10];	//0 = digital, 1 = analog
	int padMode2[10];
	int padModeE[10];	//Config/Escape mode??
	int padModeC[10];
	int padModeF[10];
	int padVib0[10];		//Command byte for small motor
	int padVib1[10];		//Command byte for large motor
	int padVibF[10][4];	//Sm motor value; Big motor value; Sm motor running?; Big motor running?
	//int padVibC[10];		//unused
	u64 padPress[10][16];//unused?
	int curPad;			//0=pad1; 1=pad2
	int curByte;		//current command/data byte
	int curCmd;			//current command from PSX/PS2
	int cmdLen;			//# of bytes in pad reply
	int irq10En[10];	// enable IRQ10 output for lightgun port
	int isConnected[10];	// is controller connected?
	int trAll[2];			// transfer all mode select
	int multiPad[2];	// which controller is connected to the multipad
} global;

extern void SysPrintf(char *fmt, ...);
extern int stop;

/* Controller type, later do this by a Variable in the GUI */
//extern char controllerType = 0; // 0 = standard, 1 = analog (analog fails on old games)
extern long  PadFlags;
extern int gLightgun;
extern int gMouse[4];

extern virtualControllers_t virtualControllers[10];

// Use to invoke func on the mapped controller with args
#define DO_CONTROL(Control,func,args...) \
	virtualControllers[Control].control->func( \
		virtualControllers[Control].number, ## args)

void assign_controller(int wv, controller_t* type, int wp);

void setIrq( u32 irq )
{
    psxHu32ref(0x1070) |= irq;
}

void lightgunInterrupt()
{
	int cursorX;
	int cursorY;
	int Control;
	WPADData* wpad = WPAD_Data(0);
	

	if (global.irq10En[0] == 0x10) Control = 0;
	else if (global.irq10En[1] == 0x10) Control = 1;
	else return;
	
	if ((global.padID[Control] != 0x31) && (global.padID[Control] != 0x63))
		return;
	
	if(screenMode == 2)	cursorX = ((wpad[virtualControllers[Control].number].ir.x*848/640 - 104));
	else cursorX = (wpad[virtualControllers[Control].number].ir.x);
				
	cursorY = (wpad[virtualControllers[Control].number].ir.y/2); 
	
	
	
	if ((cursorY > 5) && (cursorY < 220) && wpad[virtualControllers[Control].number].ir.valid){
		if (gLightgun == 5){
		gLightgun--;
		psxRegs.interrupt |= (1 << PSXINT_LIGHTGUN);
		new_dyna_set_event(PSXINT_LIGHTGUN, (Config.PsxType ? 2157: 2146)*(cursorY + (Config.PsxType ? 40 : 0)));
		return;
		}


		if (gLightgun>0){
			setIrq( SWAPu32((u32)0x400) );	
			gLightgun--;
			psxRcntWcount(0,(cursorX*(rcnts[0].rate < 5 ? 2.52 : 0.4))+ (rcnts[0].rate < 5 ? 115 : 0) );
			psxRegs.interrupt |= (1 << PSXINT_LIGHTGUN);
			new_dyna_set_event(PSXINT_LIGHTGUN, (Config.PsxType ? 2157: 2146));
		}
	}
}

static inline u8 SetSensitivity(int a, float sensitivity)
{
	a -= 128; 
	a *= sensitivity;
	if(a >= 128) a = 127; else if(a < -128) a = -128; // clamp
	return a + 128; // PSX controls range 0-255
}

static void PADsetMode (const int pad, const int mode)	//mode = 0 (digital) or 1 (analog)
{
	static const u8 padID[] = { 0x41, 0x73, 0x41, 0x79 };
	global.padMode1[pad] = mode;
	global.padVib0[pad] = 0;
	global.padVib1[pad] = 0;
	global.padVibF[pad][0] = 0;
	global.padVibF[pad][1] = 0;
	global.padID[pad] = padID[global.padMode2[pad] * 2 + mode];
}

static void UpdateState (const int pad) //Note: pad = 0 or 1
{
	const int vib0 = global.padVibF[pad][0] ? 1 : 0;
	const int vib1 = global.padVibF[pad][1] ? 1 : 0;
	int cursorX = 0x1;
	int cursorY = 0xA; 
	int curMouse;
	static int tempcursorX[4];
	static int tempcursorY[4]; 
	static int oldcursorX[4];
	static int oldcursorY[4]; 
	static BUTTONS PAD_Data;
	static WPADData* wpad;
	float sensitivity;
	int miscButton;

	//TODO: Rework/simplify the following code & reset BUTTONS when no controller in use
	int Control = pad;
#if defined(WII) && !defined(NO_BT)
	//Need to switch between Classic and WiimoteNunchuck if user swapped extensions
	if (padType[virtualControllers[Control].number] == PADTYPE_WII)
	{
		if (virtualControllers[Control].control != &controller_WiiUPro &&
			virtualControllers[Control].control != &controller_WiiUGamepad)
		{
			if (virtualControllers[Control].control == &controller_Classic &&
				!controller_Classic.available[virtualControllers[Control].number] &&
				controller_WiimoteNunchuk.available[virtualControllers[Control].number])
				assign_controller(Control, &controller_WiimoteNunchuk, virtualControllers[Control].number);
			else if (virtualControllers[Control].control == &controller_WiimoteNunchuk &&
				!controller_WiimoteNunchuk.available[virtualControllers[Control].number] &&
				controller_Classic.available[virtualControllers[Control].number])
				assign_controller(Control, &controller_Classic, virtualControllers[Control].number);
		}
	}
	
	if (lightGun && padLightgun[pad]){
		if (virtualControllers[Control].control == &controller_Wiimote || 
			virtualControllers[Control].control == &controller_WiimoteNunchuk){
				if (lightGun == LIGHTGUN_GUNCON){
					global.padID[pad] = 0x63;
					wpad = WPAD_Data(0);
					if(screenMode == 2)	cursorX = ((wpad[virtualControllers[Control].number].ir.x*848/640 - 104)/1.72) + 75;
					else cursorX = (wpad[virtualControllers[Control].number].ir.x/1.72) + 75;
					
					cursorY = (wpad[virtualControllers[Control].number].ir.y/2) + (Config.PsxType ? 48 : 25); 
					
					if (!wpad[virtualControllers[Control].number].ir.valid){
						cursorX = 0x1;
						cursorY = 0xA; 
					}
				}
				else if (lightGun == LIGHTGUN_MOUSE){
					curMouse = virtualControllers[Control].number;
					global.padID[pad] = 0x12;
					if (!gMouse[curMouse]){				
						wpad = WPAD_Data(0);
						
						if(screenMode == 2)	cursorX = wpad[curMouse].ir.x*848/640 - 104;
						else cursorX = wpad[curMouse].ir.x;
						tempcursorX[curMouse] = cursorX;					
						sensitivity = virtualControllers[Control].config->sensitivity;
						if (sensitivity < 0.1) sensitivity = 1.0;					
						cursorX = (cursorX - oldcursorX[curMouse]) * sensitivity;
						if (cursorX > 127) cursorX = 127;
						if (cursorX < -128) cursorX = -128;
						
						cursorY = wpad[curMouse].ir.y;
						tempcursorY[curMouse] = cursorY;
						cursorY = (cursorY - oldcursorY[curMouse]) * sensitivity;
						if (cursorY > 127) cursorY = 127;
						if (cursorY < -128) cursorY = -128;
						
						cursorX = (cursorX & 0xFF) | (cursorY<<8);
						
						oldcursorX[curMouse] = tempcursorX[curMouse];
						oldcursorY[curMouse] = tempcursorY[curMouse];
						tempcursorX[curMouse] = cursorX;
						
						if (!wpad[curMouse].ir.valid){
							cursorX = 0;
						}
						gMouse[curMouse] = 1;
					}
					else{
						cursorX = tempcursorX[curMouse];
					}
				}
				
				else 
					global.padID[pad] = 0x31;
			}
		else{
			if ((global.padID[pad] == 0x31) || (global.padID[pad] == 0x63) || (global.padID[pad] == 0x12))
			PADsetMode( pad, controllerType == CONTROLLERTYPE_ANALOG ? 1 : 0);
		}
	}
	else{
		if ((global.padID[pad] == 0x31) || (global.padID[pad] == 0x63) || (global.padID[pad] == 0x12))
		PADsetMode( pad, controllerType == CONTROLLERTYPE_ANALOG ? 1 : 0);
	}
#endif
	
	if (global.padMode1[pad] != controllerType)
		PADsetMode( pad, controllerType == CONTROLLERTYPE_ANALOG ? 1 : 0);

	if(virtualControllers[Control].inUse)
	{
		global.isConnected[pad] = 1;
		
		miscButton = DO_CONTROL(Control, GetKeys, (BUTTONS*)&PAD_Data, virtualControllers[Control].config);
		if (miscButton == 1)
			stop = 1;
		else if (Control == 0 || Control == 2)
			frameLimit[0] = (miscButton == 0 ? frameLimit[1] : 0);
	}
	else
	{	//TODO: Emulate no controller present in this case.
		//Reset buttons & sticks if PAD is not in use
		global.isConnected[pad] = 0;
		PAD_Data.btns.All = 0xFFFF;
		PAD_Data.leftStickX = PAD_Data.leftStickY = PAD_Data.rightStickX = PAD_Data.rightStickY = 128;
	}
	
	sensitivity = virtualControllers[Control].config->sensitivity;
	if (sensitivity < 0.1) sensitivity = 1.0;
	PAD_Data.leftStickX = SetSensitivity(PAD_Data.leftStickX, sensitivity);
	PAD_Data.leftStickY = SetSensitivity(PAD_Data.leftStickY, sensitivity);
	PAD_Data.rightStickX = SetSensitivity(PAD_Data.rightStickX, sensitivity);
	PAD_Data.rightStickY = SetSensitivity(PAD_Data.rightStickY, sensitivity);
	
	global.padStat[pad] = (((PAD_Data.btns.All>>8)&0xFF) | ( (PAD_Data.btns.All<<8) & 0xFF00 )) &0xFFFF;
	
	
	if ((global.padID[pad] == 0x31) || (global.padID[pad] == 0x63) || (global.padID[pad] == 0x12)){
		if (lightGun == LIGHTGUN_GUNCON) global.padStat[pad] |= ~0x860;
		else if (lightGun == LIGHTGUN_JUST) global.padStat[pad] |= ~0x8c0;
		else {
			if (!(global.padStat[pad] & 0x40)) // X button
				cursorX = 0;
			global.padStat[pad] |= ~0xF;
		}
			
		
		if ((pad==0) || (padType[global.curPad] == PADTYPE_MULTITAP))
		{
			lastport1.leftJoyX = cursorY & 0xFF; lastport1.leftJoyY = cursorY >> 8;
			lastport1.rightJoyX = cursorX & 0xFF; lastport1.rightJoyY = cursorX >> 8;
			lastport1.buttonStatus = global.padStat[pad];
		}
		else
		{
			lastport2.leftJoyX = cursorY & 0xFF; lastport2.leftJoyY = cursorY >> 8;
			lastport2.rightJoyX = cursorX & 0xFF; lastport2.rightJoyY = cursorX >> 8;
			lastport2.buttonStatus = global.padStat[pad];
		}
	}
	else{
		if ((pad==0) || (padType[global.curPad] == PADTYPE_MULTITAP))
		{
			lastport1.leftJoyX = PAD_Data.leftStickX; lastport1.leftJoyY = PAD_Data.leftStickY;
			lastport1.rightJoyX = PAD_Data.rightStickX; lastport1.rightJoyY = PAD_Data.rightStickY;
			lastport1.buttonStatus = global.padStat[pad];
		}
		else
		{
			lastport2.leftJoyX = PAD_Data.leftStickX; lastport2.leftJoyY = PAD_Data.leftStickY;
			lastport2.rightJoyX = PAD_Data.rightStickX; lastport2.rightJoyY = PAD_Data.rightStickY;
			lastport2.buttonStatus = global.padStat[pad];
		}
	}

	/* Small Motor */
	if ((global.padVibF[pad][2] != vib0) )
	{
		global.padVibF[pad][2] = vib0;
		if (virtualControllers[pad].control->rumble) DO_CONTROL(pad, rumble, global.padVibF[pad][0]);
	}
	/* Big Motor */
	if ((global.padVibF[pad][3] != vib1) )
	{
		global.padVibF[pad][3] = vib1;
		if (virtualControllers[pad].control->rumble) DO_CONTROL(pad, rumble, global.padVibF[pad][1]);
	}
}

long SSS_PADopen (void *p)
{
	int i;
	if (!pad_initialized)
	{
		memset (&global, 0, sizeof (global));
		memset( &lastport1, 0, sizeof(lastport1) ) ;
		memset( &lastport2, 0, sizeof(lastport2) ) ;
		for(i = 0; i < 10; i++){
			global.padStat[i] = 0xffff;
			PADsetMode (i, controllerType == CONTROLLERTYPE_ANALOG ? 1 : 0);  //port 0, analog
		}
	}
	return 0;
}

long SSS_PADclose (void)
{
	if (pad_initialized) {
  	pad_initialized=0;
	}
	return 0 ;
}

long SSS_PADquery (void)
{
	return 3;
}

unsigned char SSS_PADstartPoll (int pad)
{
	global.curPad = pad -1;
	global.curByte = 0;
	return 0xff;
}

void SSS_SetMultiPad(int pad, int mpad)
{
	if (pad)
		global.multiPad[1] = mpad+5;
	else
		global.multiPad[0] = mpad+1;
}

static const u8 cmd40[8] =
{
	0xff, 0x5a, 0x00, 0x00, 0x02, 0x00, 0x00, 0x5a
};
static const u8 cmd41[8] = //Find out what buttons are included in poll response
{
	0xff, 0x5a, 0xff, 0xff, 0x03, 0x00, 0x00, 0x5a,
};
static const u8 cmd44[8] = //Switch modes between digital and analog
{
	0xff, 0x5a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
static const u8 cmd45[8] = //Get more status info
{	//          DS         LED ON
	0xff, 0x5a, 0x03, 0x02, 0x01, 0x02, 0x01, 0x00,
};
// Following commands always issued in sequence: 46, 46, 47, 4C, 4C; Also, only works in config mode (0xF3)
static const u8 cmd46[8] = //Read unknown constant value from controller (called twice)
{	//Note this is the first 5 bytes for Katana Wireless pad.
	//May need to change or implement 2nd response, which is indicated by 4th command byte
	0xff, 0x5a, 0x00, 0x00, 0x01, 0x02, 0x00, 0x0a,
};
static const u8 cmd47[8] = //Read unknown constant value from controller (called once)
{	//Note this is response for Katana Wireless pad
	0xff, 0x5a, 0x00, 0x00, 0x02, 0x00, 0x01, 0x00,
};
static const u8 cmd4c[8] = //Read unknown constant value from controller (called twice)
{	//Note this response seems to be incorrect. May also need to implement 1st/2nd responses
	0xff, 0x5a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
static const u8 cmd4d[8] = //Map bytes in 42 command to actuate motors; only works in config mode (0xF3)
{	//These data bytes should be changed to 00 or 01 if currently mapped to a motor
	0xff, 0x5a, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
};
static const u8 cmd4f[8] = //Enable/disable digital/analog responses bits; only works in config mode (0xF3)
{	//            FF    FF    03 <- each bit here corresponds to response byte
	0xff, 0x5a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5a,
};

unsigned char multitap[34] = { 0x80, 0x5a,
									0x41, 0x5a, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
									0x41, 0x5a, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
									0x41, 0x5a, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
									0x41, 0x5a, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
									
unsigned char SSS_PADpoll (const unsigned char value)
{
	int i, offset, offsetSlot;
	int pad = global.curPad;
	int slot = pad;
	if (padType[slot] == PADTYPE_MULTITAP)
		pad = global.multiPad[slot];
	
	const int cur = global.curByte;

//Pragma to avoid packing on "buffer" union
//Not sure if necessary on PPC
#pragma pack(push,1)
	union buffer
	{
		u16 b16[20];
		u8  b8[40];
	};

	static union buffer buf;
	if (cur == 0)
	{
		global.curByte++;
		global.curCmd = value;
		if (controllerType != CONTROLLERTYPE_ANALOG)
		{
			if (value != 0x42)
				if (padType[slot] == PADTYPE_MULTITAP)
					return 0xFF;
				else
				global.curCmd = 0x42;
		}
		switch (global.curCmd)
		{
		case 0x40:
			global.cmdLen = sizeof (cmd40);
			memcpy (buf.b8, cmd40, sizeof (cmd40));
			return 0xf3;
		case 0x41:
			global.cmdLen = sizeof (cmd41);
			memcpy (buf.b8, cmd41, sizeof (cmd41));
			return 0xf3;
		case 0x42:
			if (padType[slot] == PADTYPE_MULTITAP){
				if (global.trAll[slot] == 1){
					global.cmdLen = sizeof (multitap);
					memcpy (buf.b8, multitap, sizeof (multitap));
					offsetSlot = 2+(slot*4);
					for(i = 0; i < 4; i++) {
						UpdateState (i+offsetSlot);
						offset = i*8;
						if (global.isConnected[i+offsetSlot]) {
							buf.b8[2+offset] = global.padID[i+offsetSlot];
							}
						else {
							buf.b8[2+offset] = 0xFF;
							buf.b8[3+offset] = 0xFF;
							}
						buf.b8[6+offset] = lastport1.rightJoyX ;
						buf.b8[7+offset] = lastport1.rightJoyY ;
						buf.b8[8+offset] = lastport1.leftJoyX ;
						buf.b8[9+offset] = lastport1.leftJoyY ;
						buf.b16[2+(i*4)] = global.padStat[i+offsetSlot];
					}
					return 0x80;
				}
			}
			UpdateState(pad);
			
		case 0x43:
			global.cmdLen = 2 + 2 * (global.padID[pad] & 0x0f);
			buf.b8[1] = global.padModeC[pad] ? 0x00 : 0x5a;
			buf.b16[1] = global.padStat[pad];
			if (value == 0x43 && global.padModeE[pad])
			{
				buf.b16[2] = 0;
				buf.b16[3] = 0;
				return 0xf3;
			}
			else
			{
				if (padType[slot] != PADTYPE_MULTITAP){
					buf.b8[4] = pad ? lastport2.rightJoyX : lastport1.rightJoyX ;
					buf.b8[5] = pad ? lastport2.rightJoyY : lastport1.rightJoyY ;
					buf.b8[6] = pad ? lastport2.leftJoyX : lastport1.leftJoyX ;
					buf.b8[7] = pad ? lastport2.leftJoyY : lastport1.leftJoyY ;
				}
				else{
					buf.b8[4] = lastport1.rightJoyX ;
					buf.b8[5] = lastport1.rightJoyY ;
					buf.b8[6] = lastport1.leftJoyX ;
					buf.b8[7] = lastport1.leftJoyY ;
				}
					
				//if (global.padID[pad] == 0x79)
				//{
  				// do some pressure stuff (this is for PS2 only!)
				//}
				return (u8)global.padID[pad];
			}
			break;
		case 0x44:
			global.cmdLen = sizeof (cmd44);
			memcpy (buf.b8, cmd44, sizeof (cmd44));
			return 0xf3;
		case 0x45:
			global.cmdLen = sizeof (cmd45);
			memcpy (buf.b8, cmd45, sizeof (cmd45));
			buf.b8[4] = (u8)global.padMode1[pad];
			return 0xf3;
		case 0x46:
			global.cmdLen = sizeof (cmd46);
			memcpy (buf.b8, cmd46, sizeof (cmd46));
			return 0xf3;
		case 0x47:
			global.cmdLen = sizeof (cmd47);
			memcpy (buf.b8, cmd47, sizeof (cmd47));
			return 0xf3;
		case 0x4c:
			global.cmdLen = sizeof (cmd4c);
			memcpy (buf.b8, cmd4c, sizeof (cmd4c));
			return 0xf3;
		case 0x4d:
			global.cmdLen = sizeof (cmd4d);
			memcpy (buf.b8, cmd4d, sizeof (cmd4d));
			return 0xf3;
		case 0x4f:
			global.padID[pad] = 0x79;
			global.padMode2[pad] = 1;
			global.cmdLen = sizeof (cmd4f);
			memcpy (buf.b8, cmd4f, sizeof (cmd4f));
			return 0xf3;
		}
	}
	switch (global.curCmd)
	{
	case 0x42:
		if (cur == global.padVib0[pad])
			global.padVibF[pad][0] = value;
		if (cur == global.padVib1[pad])
			global.padVibF[pad][1] = value;
		if (cur == 2)
			global.irq10En[slot] = value;
		if (cur == 1 && padType[slot] == PADTYPE_MULTITAP)
			global.trAll[slot] = value & 1;
		break;
	case 0x43:
		if (cur == 2)
		{
			global.padModeE[pad] = value; // cmd[3]==1 ? enter : exit escape mode
			global.padModeC[pad] = 0;
		}
		break;
	case 0x44:
		if (cur == 2)
			PADsetMode (pad, value);	// cmd[3]==1 ? analog : digital
		if (cur == 3)
			global.padModeF[pad] = (value == 3); //cmd[4]==3 ? lock : don't log analog/digital button
		break;
	case 0x46:
		if (cur == 2)
		{
			switch(value)
			{
			case 0x00:
				buf.b8[5] = 0x02;
				buf.b8[6] = 0x00;
				buf.b8[7] = 0x0A;
				break;
			case 0x01:
				buf.b8[5] = 0x01;
				buf.b8[6] = 0x01;
				buf.b8[7] = 0x14;
				break;
			}
		}
		break;
	case 0x4c:
		if (cur == 2)
		{
			static const u8 buf5[] = { 0x04, 0x07, 0x02, 0x05 };
			buf.b8[5] = buf5[value & 0x03];
		}
		break;
	case 0x4d:
		if (cur >= 2)
		{
			if (cur == global.padVib0[pad])
				buf.b8[cur] = 0x00;
			else if (cur == global.padVib1[pad])
				buf.b8[cur] = 0x01;

			switch (value)
			{
			case 0x00:
				global.padVib0[pad] = cur;
			case 0x01:
				if ((global.padID[pad] & 0x0f) < (cur - 1) / 2)
					 global.padID[pad] = (global.padID[pad] & 0xf0) + (cur - 1) / 2;
				if(value) global.padVib1[pad] = cur;
			}
		}
		break;
	}
	
		
	if (cur >= global.cmdLen)
		return 0;
	return buf.b8[global.curByte++];
//Revert packing
#pragma pack(pop)
}

long SSS_PADreadPort1 (PadDataS* pads)
{
	//#PADreadPort1 not used in PCSX
/*
	pads->buttonStatus = global.padStat[0];

	memset (pads, 0, sizeof (PadDataS));
	if ((global.padID[0] & 0xf0) == 0x40)
	{
		pads->rightJoyX = pads->rightJoyY = pads->leftJoyX = pads->leftJoyY = 128 ;
		pads->controllerType = PSE_PAD_TYPE_STANDARD;
	}
	else
	{
		pads->controllerType = PSE_PAD_TYPE_ANALOGPAD;
		int Control = 0;
#if defined(WII) && !defined(NO_BT)
		//Need to switch between Classic and WiimoteNunchuck if user swapped extensions
		if (padType[virtualControllers[Control].number] == PADTYPE_WII)
		{
			if (virtualControllers[Control].control == &controller_Classic &&
				!controller_Classic.available[virtualControllers[Control].number] &&
				controller_WiimoteNunchuk.available[virtualControllers[Control].number])
				assign_controller(Control, &controller_WiimoteNunchuk, virtualControllers[Control].number);
			else if (virtualControllers[Control].control == &controller_WiimoteNunchuk &&
				!controller_WiimoteNunchuk.available[virtualControllers[Control].number] &&
				controller_Classic.available[virtualControllers[Control].number])
				assign_controller(Control, &controller_Classic, virtualControllers[Control].number);
		}
#endif
		if(virtualControllers[Control].inUse)
			if(DO_CONTROL(Control, GetKeys, (BUTTONS*)&PAD_1, virtualControllers[Control].config))
				stop = 1;

		pads->leftJoyX = PAD_1.leftStickX; pads->leftJoyY = PAD_1.leftStickY;
		pads->rightJoyX = PAD_1.rightStickX; pads->rightJoyY = PAD_1.rightStickY;
	}

	memcpy( &lastport1, pads, sizeof( lastport1 ) ) ;
*/
	return 0;
}

long SSS_PADreadPort2 (PadDataS* pads)
{
	//#PADreadPort2 not used in PCSX
/*
	pads->buttonStatus = global.padStat[1];

	memset (pads, 0, sizeof (PadDataS));
	if ((global.padID[1] & 0xf0) == 0x40)
	{
		pads->rightJoyX = pads->rightJoyY = pads->leftJoyX = pads->leftJoyY = 128 ;
		pads->controllerType = PSE_PAD_TYPE_STANDARD;
	}
	else
	{
		pads->controllerType = PSE_PAD_TYPE_ANALOGPAD;
		int Control = 1;
#if defined(WII) && !defined(NO_BT)
		//Need to switch between Classic and WiimoteNunchuck if user swapped extensions
		if (padType[virtualControllers[Control].number] == PADTYPE_WII)
		{
			if (virtualControllers[Control].control == &controller_Classic &&
				!controller_Classic.available[virtualControllers[Control].number] &&
				controller_WiimoteNunchuk.available[virtualControllers[Control].number])
				assign_controller(Control, &controller_WiimoteNunchuk, virtualControllers[Control].number);
			else if (virtualControllers[Control].control == &controller_WiimoteNunchuk &&
				!controller_WiimoteNunchuk.available[virtualControllers[Control].number] &&
				controller_Classic.available[virtualControllers[Control].number])
				assign_controller(Control, &controller_Classic, virtualControllers[Control].number);
		}
#endif
		if(virtualControllers[Control].inUse)
			if(DO_CONTROL(Control, GetKeys, (BUTTONS*)&PAD_2, virtualControllers[Control].config))
				stop = 1;

		pads->leftJoyX = PAD_2.leftStickX; pads->leftJoyY = PAD_2.leftStickY;
		pads->rightJoyX = PAD_2.rightStickX; pads->rightJoyY = PAD_2.rightStickY;
	}

	memcpy( &lastport2, pads, sizeof( lastport1 ) ) ;
*/
	return 0;
}
