/**
 * WiiSX - ConfigureInputFrame.cpp
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

#include "MenuContext.h"
#include "SettingsFrame.h"
#include "ConfigureInputFrame.h"
#include "../libgui/Button.h"
#include "../libgui/TextBox.h"
#include "../libgui/resources.h"
#include "../libgui/FocusManager.h"
#include "../libgui/CursorManager.h"
//#include "../libgui/MessageBox.h"
//#include "../main/timers.h"
//#include "../main/wii64config.h"

extern "C" {
#include "../gc_input/controller.h"
}

void Func_AutoSelectInput();
void Func_ManualSelectInput();

void Func_TogglePad0Type();
void Func_TogglePad1Type();
void Func_TogglePad0AType();
void Func_TogglePad0BType();
void Func_TogglePad0CType();
void Func_TogglePad0DType();
void Func_TogglePad1AType();
void Func_TogglePad1BType();
void Func_TogglePad1CType();
void Func_TogglePad1DType();

void Func_TogglePad0Assign();
void Func_TogglePad1Assign();
void Func_TogglePad0AAssign();
void Func_TogglePad0BAssign();
void Func_TogglePad0CAssign();
void Func_TogglePad0DAssign();
void Func_TogglePad1AAssign();
void Func_TogglePad1BAssign();
void Func_TogglePad1CAssign();
void Func_TogglePad1DAssign();

void Func_ReturnFromConfigureInputFrame();


#define NUM_FRAME_BUTTONS 22
#define FRAME_BUTTONS configureInputFrameButtons
#define FRAME_STRINGS configureInputFrameStrings
#define NUM_FRAME_TEXTBOXES 5
#define FRAME_TEXTBOXES configureInputFrameTextBoxes

static char FRAME_STRINGS[17][15] =
	{ "Pad Assignment",
	  "PSX Pad 1",
	  "PSX Pad 2",
	  "Multitap 1",
	  "Multitap 2",

	  "Automatic",
	  "Manual",
	  "None",
	  "Gamecube Pad",
	  "Wii Pad",
	  "Multitap",
	  "Auto Assign",
	  "",
	  "1",
	  "2",
	  "3",
	  "4"};

struct ButtonInfo
{
	menu::Button	*button;
	int				buttonStyle;
	char*			buttonString;
	float			x;
	float			y;
	float			width;
	float			height;
	int				focusUp;
	int				focusDown;
	int				focusLeft;
	int				focusRight;
	ButtonFunc		clickedFunc;
	ButtonFunc		returnFunc;
} FRAME_BUTTONS[NUM_FRAME_BUTTONS] =
{ //	button	buttonStyle buttonString		x		y		width	height	Up	Dwn	Lft	Rt	clickFunc				returnFunc
	{	NULL,	BTN_A_SEL,	FRAME_STRINGS[5],	240.0,	 30.0,	135.0,	56.0,	 17, 2,	 1,	 1,	Func_AutoSelectInput,	Func_ReturnFromConfigureInputFrame }, // Automatic Pad Assignment
	{	NULL,	BTN_A_SEL,	FRAME_STRINGS[6],	395.0,	 30.0,	120.0,	56.0,	 21, 3,	 0,	 0,	Func_ManualSelectInput,	Func_ReturnFromConfigureInputFrame }, // Manual Pad Assignment
	{	NULL,	BTN_A_NRM,	FRAME_STRINGS[11],	30.0,	135.0,	200.0,	56.0,	 0,	 4,	 13, 12,Func_TogglePad0Type,	Func_ReturnFromConfigureInputFrame }, // Toggle Pad 0 Type
	{	NULL,	BTN_A_NRM,	FRAME_STRINGS[11],	330.0,	135.0,	200.0,	56.0,	 1,	 8,	 12, 13,Func_TogglePad1Type,	Func_ReturnFromConfigureInputFrame }, // Toggle Pad 1 Type
	{	NULL,	BTN_A_NRM,	FRAME_STRINGS[11],	30.0,	250.0,	200.0,	56.0,	 3,	 5,	 18, 14,Func_TogglePad0AType,	Func_ReturnFromConfigureInputFrame }, // Toggle Pad 0A Type
	{	NULL,	BTN_A_NRM,	FRAME_STRINGS[11],	30.0,	306.0,	200.0,	56.0,	 4,	 6,	 19, 15,Func_TogglePad0BType,	Func_ReturnFromConfigureInputFrame }, // Toggle Pad 0B Type
	{	NULL,	BTN_A_NRM,	FRAME_STRINGS[11],	30.0,	362.0,	200.0,	56.0,	 5,	 7,	 20, 16,Func_TogglePad0CType,	Func_ReturnFromConfigureInputFrame }, // Toggle Pad 0C Type
	{	NULL,	BTN_A_NRM,	FRAME_STRINGS[11],	30.0,	418.0,	200.0,	56.0,	 6,	 0,	 21, 17,Func_TogglePad0DType,	Func_ReturnFromConfigureInputFrame }, // Toggle Pad 0D Type
	{	NULL,	BTN_A_NRM,	FRAME_STRINGS[11],	330.0,	250.0,	200.0,	56.0,	 3,	 9,	 14, 18,Func_TogglePad1AType,	Func_ReturnFromConfigureInputFrame }, // Toggle Pad 1A Type
	{	NULL,	BTN_A_NRM,	FRAME_STRINGS[11],	330.0,	306.0,	200.0,	56.0,	 8,	 10, 15, 19,Func_TogglePad1BType,	Func_ReturnFromConfigureInputFrame }, // Toggle Pad 1B Type
	{	NULL,	BTN_A_NRM,	FRAME_STRINGS[11],	330.0,	362.0,	200.0,	56.0,	 9,	 11, 16, 20,Func_TogglePad1CType,	Func_ReturnFromConfigureInputFrame }, // Toggle Pad 1C Type
	{	NULL,	BTN_A_NRM,	FRAME_STRINGS[11],	330.0,	418.0,	200.0,	56.0,	 10, 1,	 17, 21,Func_TogglePad1DType,	Func_ReturnFromConfigureInputFrame }, // Toggle Pad 1D Type
	{	NULL,	BTN_A_NRM,	FRAME_STRINGS[12],	250.0,	135.0,	 55.0,	56.0,	 0,	 14, 2,	 3,	Func_TogglePad0Assign,	Func_ReturnFromConfigureInputFrame }, // Toggle Pad 0 Assignment
	{	NULL,	BTN_A_NRM,	FRAME_STRINGS[12],	550.0,	135.0,	 55.0,	56.0,	 1,	 18, 3,	 2,	Func_TogglePad1Assign,	Func_ReturnFromConfigureInputFrame }, // Toggle Pad 1 Assignment
	{	NULL,	BTN_A_NRM,	FRAME_STRINGS[12],	250.0,	250.0,	 55.0,	56.0,	 12, 15, 4,	 8,	Func_TogglePad0AAssign,	Func_ReturnFromConfigureInputFrame }, // Toggle Pad 0A Assignment
	{	NULL,	BTN_A_NRM,	FRAME_STRINGS[12],	250.0,	306.0,	 55.0,	56.0,	 14, 16, 5,	 9,	Func_TogglePad0BAssign,	Func_ReturnFromConfigureInputFrame }, // Toggle Pad 0B Assignment
	{	NULL,	BTN_A_NRM,	FRAME_STRINGS[12],	250.0,	362.0,	 55.0,	56.0,	 15, 17, 6,	 10,Func_TogglePad0CAssign,	Func_ReturnFromConfigureInputFrame }, // Toggle Pad 0C Assignment
	{	NULL,	BTN_A_NRM,	FRAME_STRINGS[12],	250.0,	418.0,	 55.0,	56.0,	 16, 0,	 7,	 11,Func_TogglePad0DAssign,	Func_ReturnFromConfigureInputFrame }, // Toggle Pad 0D Assignment
	{	NULL,	BTN_A_NRM,	FRAME_STRINGS[12],	550.0,	250.0,	 55.0,	56.0,	 13, 19, 8,	 4,	Func_TogglePad1AAssign,	Func_ReturnFromConfigureInputFrame }, // Toggle Pad 1A Assignment
	{	NULL,	BTN_A_NRM,	FRAME_STRINGS[12],	550.0,	306.0,	 55.0,	56.0,	 18, 20, 9,	 5,	Func_TogglePad1BAssign,	Func_ReturnFromConfigureInputFrame }, // Toggle Pad 1B Assignment
	{	NULL,	BTN_A_NRM,	FRAME_STRINGS[12],	550.0,	362.0,	 55.0,	56.0,	 19, 21, 10, 6,	Func_TogglePad1CAssign,	Func_ReturnFromConfigureInputFrame }, // Toggle Pad 1C Assignment
	{	NULL,	BTN_A_NRM,	FRAME_STRINGS[12],	550.0,	418.0,	 55.0,	56.0,	 20, 1,	 11, 7,	Func_TogglePad1DAssign,	Func_ReturnFromConfigureInputFrame }, // Toggle Pad 1D Assignment
};

struct TextBoxInfo
{
	menu::TextBox	*textBox;
	char*			textBoxString;
	float			x;
	float			y;
	float			scale;
	bool			centered;
} FRAME_TEXTBOXES[NUM_FRAME_TEXTBOXES] =
{ //	textBox	textBoxString		x		y		scale	centered
	{	NULL,	FRAME_STRINGS[0],	125.0,	68.0,	 1.0,	true }, // Pad Assignment
	{	NULL,	FRAME_STRINGS[1],	125.0,	115.0,	 1.0,	true }, // Pad 1
	{	NULL,	FRAME_STRINGS[2],	425.0,	115.0,	 1.0,	true }, // Pad 2
	{	NULL,	FRAME_STRINGS[3],	125.0,	228.0,	 1.0,	true }, // Multitap 1
	{	NULL,	FRAME_STRINGS[4],	425.0,	228.0,	 1.0,	true }, // Multitap 2
};

ConfigureInputFrame::ConfigureInputFrame()
{
	for (int i = 0; i < NUM_FRAME_BUTTONS; i++)
		FRAME_BUTTONS[i].button = new menu::Button(FRAME_BUTTONS[i].buttonStyle, &FRAME_BUTTONS[i].buttonString, 
										FRAME_BUTTONS[i].x, FRAME_BUTTONS[i].y, 
										FRAME_BUTTONS[i].width, FRAME_BUTTONS[i].height);

	for (int i = 0; i < NUM_FRAME_BUTTONS; i++)
	{
		if (FRAME_BUTTONS[i].focusUp != -1) FRAME_BUTTONS[i].button->setNextFocus(menu::Focus::DIRECTION_UP, FRAME_BUTTONS[FRAME_BUTTONS[i].focusUp].button);
		if (FRAME_BUTTONS[i].focusDown != -1) FRAME_BUTTONS[i].button->setNextFocus(menu::Focus::DIRECTION_DOWN, FRAME_BUTTONS[FRAME_BUTTONS[i].focusDown].button);
		if (FRAME_BUTTONS[i].focusLeft != -1) FRAME_BUTTONS[i].button->setNextFocus(menu::Focus::DIRECTION_LEFT, FRAME_BUTTONS[FRAME_BUTTONS[i].focusLeft].button);
		if (FRAME_BUTTONS[i].focusRight != -1) FRAME_BUTTONS[i].button->setNextFocus(menu::Focus::DIRECTION_RIGHT, FRAME_BUTTONS[FRAME_BUTTONS[i].focusRight].button);
		FRAME_BUTTONS[i].button->setActive(true);
		if (FRAME_BUTTONS[i].clickedFunc) FRAME_BUTTONS[i].button->setClicked(FRAME_BUTTONS[i].clickedFunc);
		if (FRAME_BUTTONS[i].returnFunc) FRAME_BUTTONS[i].button->setReturn(FRAME_BUTTONS[i].returnFunc);
		add(FRAME_BUTTONS[i].button);
		menu::Cursor::getInstance().addComponent(this, FRAME_BUTTONS[i].button, FRAME_BUTTONS[i].x, 
												FRAME_BUTTONS[i].x+FRAME_BUTTONS[i].width, FRAME_BUTTONS[i].y, 
												FRAME_BUTTONS[i].y+FRAME_BUTTONS[i].height);
	}

	for (int i = 0; i < NUM_FRAME_TEXTBOXES; i++)
	{
		FRAME_TEXTBOXES[i].textBox = new menu::TextBox(&FRAME_TEXTBOXES[i].textBoxString, 
										FRAME_TEXTBOXES[i].x, FRAME_TEXTBOXES[i].y, 
										FRAME_TEXTBOXES[i].scale, FRAME_TEXTBOXES[i].centered);
		add(FRAME_TEXTBOXES[i].textBox);
	}

	setDefaultFocus(FRAME_BUTTONS[0].button);
	setBackFunc(Func_ReturnFromConfigureInputFrame);
	setEnabled(true);
	activateSubmenu(SUBMENU_REINIT);
}

ConfigureInputFrame::~ConfigureInputFrame()
{
	for (int i = 0; i < NUM_FRAME_TEXTBOXES; i++)
		delete FRAME_TEXTBOXES[i].textBox;
	for (int i = 0; i < NUM_FRAME_BUTTONS; i++)
	{
		menu::Cursor::getInstance().removeComponent(this, FRAME_BUTTONS[i].button);
		delete FRAME_BUTTONS[i].button;
	}

}

void ConfigureInputFrame::activateSubmenu(int submenu)
{
	if (padAutoAssign == PADAUTOASSIGN_AUTOMATIC)
	{
		FRAME_BUTTONS[0].button->setSelected(true);
		FRAME_BUTTONS[1].button->setSelected(false);
		FRAME_BUTTONS[0].button->setNextFocus(menu::Focus::DIRECTION_DOWN, NULL);
		FRAME_BUTTONS[0].button->setNextFocus(menu::Focus::DIRECTION_UP, NULL);
		FRAME_BUTTONS[1].button->setNextFocus(menu::Focus::DIRECTION_DOWN, NULL);
		FRAME_BUTTONS[1].button->setNextFocus(menu::Focus::DIRECTION_UP, NULL);
		for (int i = 0; i < 10; i++)
		{
			FRAME_BUTTONS[i+2].button->setActive(false);
			FRAME_BUTTONS[i+2].buttonString = FRAME_STRINGS[11];
			if (i>1)	FRAME_BUTTONS[i+2].buttonString = FRAME_STRINGS[12];
			FRAME_BUTTONS[i+12].button->setActive(false);
			FRAME_BUTTONS[i+12].buttonString = FRAME_STRINGS[12];
		}
	}
	else
	{
		FRAME_BUTTONS[0].button->setSelected(false);
		FRAME_BUTTONS[1].button->setSelected(true);
		for (int i = 0; i < NUM_FRAME_BUTTONS; i++)
		{
			if (FRAME_BUTTONS[i].focusUp != -1) FRAME_BUTTONS[i].button->setNextFocus(menu::Focus::DIRECTION_UP, FRAME_BUTTONS[FRAME_BUTTONS[i].focusUp].button);
			if (FRAME_BUTTONS[i].focusDown != -1) FRAME_BUTTONS[i].button->setNextFocus(menu::Focus::DIRECTION_DOWN, FRAME_BUTTONS[FRAME_BUTTONS[i].focusDown].button);
			if (FRAME_BUTTONS[i].focusLeft != -1) FRAME_BUTTONS[i].button->setNextFocus(menu::Focus::DIRECTION_LEFT, FRAME_BUTTONS[FRAME_BUTTONS[i].focusLeft].button);
			if (FRAME_BUTTONS[i].focusRight != -1) FRAME_BUTTONS[i].button->setNextFocus(menu::Focus::DIRECTION_RIGHT, FRAME_BUTTONS[FRAME_BUTTONS[i].focusRight].button);
		}
		for (int i = 0; i < 10; i++)
		{
			FRAME_BUTTONS[i+2].button->setActive(true);
			FRAME_BUTTONS[i+2].buttonString = FRAME_STRINGS[padType[i]+7];
			FRAME_BUTTONS[i+12].button->setActive(true);
			FRAME_BUTTONS[i+12].buttonString = FRAME_STRINGS[padAssign[i]+13];
		}
		
		
	}
	
	if (padType[0] != PADTYPE_MULTITAP){
		for (int i = 2; i < 6; i++){
			FRAME_BUTTONS[i+2].button->setActive(false);
			FRAME_BUTTONS[i+2].buttonString = FRAME_STRINGS[12];
			FRAME_BUTTONS[i+12].button->setActive(false);
			FRAME_BUTTONS[i+12].buttonString = FRAME_STRINGS[12];
		}
	}
	if (padType[1] != PADTYPE_MULTITAP){
		for (int i = 6; i < 10; i++){
			FRAME_BUTTONS[i+2].button->setActive(false);
			FRAME_BUTTONS[i+2].buttonString = FRAME_STRINGS[12];
			FRAME_BUTTONS[i+12].button->setActive(false);
			FRAME_BUTTONS[i+12].buttonString = FRAME_STRINGS[12];
		}
			
	}
}

extern MenuContext *pMenuContext;

void Func_AutoSelectInput()
{
	padAutoAssign = PADAUTOASSIGN_AUTOMATIC;
	pMenuContext->getFrame(MenuContext::FRAME_CONFIGUREINPUT)->activateSubmenu(ConfigureInputFrame::SUBMENU_REINIT);
}

void Func_ManualSelectInput()
{
	padAutoAssign = PADAUTOASSIGN_MANUAL;
	pMenuContext->getFrame(MenuContext::FRAME_CONFIGUREINPUT)->activateSubmenu(ConfigureInputFrame::SUBMENU_REINIT);
}

void Func_AssignPad(int i)
{
	controller_t* type = NULL;
	switch (padType[i])
	{
	case PADTYPE_GAMECUBE:
		type = &controller_GC;
		break;
#ifdef HW_RVL
	case PADTYPE_WII:
		if (controller_WiiUPro.available[(int)padAssign[i]])
			type = &controller_WiiUPro;
		else if (controller_WiiUGamepad.available[(int)padAssign[i]])
			type = &controller_WiiUGamepad;
		//Note: Wii expansion detection is done in InputStatusBar.cpp during MainFrame draw
		else if (controller_Classic.available[(int)padAssign[i]])
			type = &controller_Classic;
		else
			type = &controller_WiimoteNunchuk;
		break;
#endif
	case PADTYPE_NONE:
		unassign_controller(i);
		return;
	}
		assign_controller(i, type, (int) padAssign[i]);
}

void Func_TogglePad0Type()
{
	int i = PADASSIGN_INPUT0;
#ifdef HW_RVL
	padType[i] = (padType[i]+1) %4;
#else
	padType[i] = (padType[i]+1) %3;
#endif

	if (padType[i] == PADTYPE_MULTITAP){ 
		unassign_controller(i);
		for(int j = 2;  j < 6; j++)
			Func_AssignPad(j);
	}
	else if (padType[i]) Func_AssignPad(i);
	else			unassign_controller(i);
	pMenuContext->getFrame(MenuContext::FRAME_CONFIGUREINPUT)->activateSubmenu(ConfigureInputFrame::SUBMENU_REINIT);
}


void Func_TogglePad1Type()
{
	int i = PADASSIGN_INPUT1;
#ifdef HW_RVL
	padType[i] = (padType[i]+1) %4;
#else
	padType[i] = (padType[i]+1) %3;
#endif
	
	if (padType[i] == PADTYPE_MULTITAP){ 
		unassign_controller(i);
		for(int j = 6;  j < 10; j++)
			Func_AssignPad(j);
	}
	else if (padType[i]) Func_AssignPad(i);
	else			unassign_controller(i);
	pMenuContext->getFrame(MenuContext::FRAME_CONFIGUREINPUT)->activateSubmenu(ConfigureInputFrame::SUBMENU_REINIT);
}

void Func_TogglePad0Assign()
{
	int i = PADASSIGN_INPUT0;
	padAssign[i] = (padAssign[i]+1) %4;

	if (padType[i] && padType[i] != PADTYPE_MULTITAP) Func_AssignPad(i);

	pMenuContext->getFrame(MenuContext::FRAME_CONFIGUREINPUT)->activateSubmenu(ConfigureInputFrame::SUBMENU_REINIT);
}

void Func_TogglePad1Assign()
{
	int i = PADASSIGN_INPUT1;
	padAssign[i] = (padAssign[i]+1) %4;

	if (padType[i] && padType[i] != PADTYPE_MULTITAP) Func_AssignPad(i);

	pMenuContext->getFrame(MenuContext::FRAME_CONFIGUREINPUT)->activateSubmenu(ConfigureInputFrame::SUBMENU_REINIT);
}

void Func_ReturnFromConfigureInputFrame()
{
	pMenuContext->setActiveFrame(MenuContext::FRAME_SETTINGS,SettingsFrame::SUBMENU_INPUT);
}


//////////////////////////////////
//		Multitap functions		//
//////////////////////////////////

void Func_TogglePad0AType()
{
	int i = PADASSIGN_INPUT0A;
#ifdef HW_RVL
	padType[i] = (padType[i]+1) %3;
#else
	padType[i] = (padType[i]+1) %2;
#endif

	if (padType[i]) Func_AssignPad(i);
	else			unassign_controller(i);
	pMenuContext->getFrame(MenuContext::FRAME_CONFIGUREINPUT)->activateSubmenu(ConfigureInputFrame::SUBMENU_REINIT);
}

void Func_TogglePad0BType()
{
	int i = PADASSIGN_INPUT0B;
#ifdef HW_RVL
	padType[i] = (padType[i]+1) %3;
#else
	padType[i] = (padType[i]+1) %2;
#endif

	if (padType[i]) Func_AssignPad(i);
	else			unassign_controller(i);
	pMenuContext->getFrame(MenuContext::FRAME_CONFIGUREINPUT)->activateSubmenu(ConfigureInputFrame::SUBMENU_REINIT);
}

void Func_TogglePad0CType()
{
	int i = PADASSIGN_INPUT0C;
#ifdef HW_RVL
	padType[i] = (padType[i]+1) %3;
#else
	padType[i] = (padType[i]+1) %2;
#endif

	if (padType[i]) Func_AssignPad(i);
	else			unassign_controller(i);
	pMenuContext->getFrame(MenuContext::FRAME_CONFIGUREINPUT)->activateSubmenu(ConfigureInputFrame::SUBMENU_REINIT);
}

void Func_TogglePad0DType()
{
	int i = PADASSIGN_INPUT0D;
#ifdef HW_RVL
	padType[i] = (padType[i]+1) %3;
#else
	padType[i] = (padType[i]+1) %2;
#endif

	if (padType[i]) Func_AssignPad(i);
	else			unassign_controller(i);
	pMenuContext->getFrame(MenuContext::FRAME_CONFIGUREINPUT)->activateSubmenu(ConfigureInputFrame::SUBMENU_REINIT);
}

///////////////////////////////////////////// Second Pad Type

void Func_TogglePad1AType()
{
	int i = PADASSIGN_INPUT1A;
#ifdef HW_RVL
	padType[i] = (padType[i]+1) %3;
#else
	padType[i] = (padType[i]+1) %2;
#endif

	if (padType[i]) Func_AssignPad(i);
	else			unassign_controller(i);
	pMenuContext->getFrame(MenuContext::FRAME_CONFIGUREINPUT)->activateSubmenu(ConfigureInputFrame::SUBMENU_REINIT);
}

void Func_TogglePad1BType()
{
	int i = PADASSIGN_INPUT1B;
#ifdef HW_RVL
	padType[i] = (padType[i]+1) %3;
#else
	padType[i] = (padType[i]+1) %2;
#endif

	if (padType[i]) Func_AssignPad(i);
	else			unassign_controller(i);
	pMenuContext->getFrame(MenuContext::FRAME_CONFIGUREINPUT)->activateSubmenu(ConfigureInputFrame::SUBMENU_REINIT);
}

void Func_TogglePad1CType()
{
	int i = PADASSIGN_INPUT1C;
#ifdef HW_RVL
	padType[i] = (padType[i]+1) %3;
#else
	padType[i] = (padType[i]+1) %2;
#endif

	if (padType[i]) Func_AssignPad(i);
	else			unassign_controller(i);
	pMenuContext->getFrame(MenuContext::FRAME_CONFIGUREINPUT)->activateSubmenu(ConfigureInputFrame::SUBMENU_REINIT);
}

void Func_TogglePad1DType()
{
	int i = PADASSIGN_INPUT1D;
#ifdef HW_RVL
	padType[i] = (padType[i]+1) %3;
#else
	padType[i] = (padType[i]+1) %2;
#endif

	if (padType[i]) Func_AssignPad(i);
	else			unassign_controller(i);
	pMenuContext->getFrame(MenuContext::FRAME_CONFIGUREINPUT)->activateSubmenu(ConfigureInputFrame::SUBMENU_REINIT);
}


///////////////////////////////////////////// Assign

void Func_TogglePad0AAssign()
{
	int i = PADASSIGN_INPUT0A;
	padAssign[i] = (padAssign[i]+1) %4;

	if (padType[i]) Func_AssignPad(i);

	pMenuContext->getFrame(MenuContext::FRAME_CONFIGUREINPUT)->activateSubmenu(ConfigureInputFrame::SUBMENU_REINIT);
}

void Func_TogglePad0BAssign()
{
	int i = PADASSIGN_INPUT0B;
	padAssign[i] = (padAssign[i]+1) %4;

	if (padType[i]) Func_AssignPad(i);

	pMenuContext->getFrame(MenuContext::FRAME_CONFIGUREINPUT)->activateSubmenu(ConfigureInputFrame::SUBMENU_REINIT);
}

void Func_TogglePad0CAssign()
{
	int i = PADASSIGN_INPUT0C;
	padAssign[i] = (padAssign[i]+1) %4;

	if (padType[i]) Func_AssignPad(i);

	pMenuContext->getFrame(MenuContext::FRAME_CONFIGUREINPUT)->activateSubmenu(ConfigureInputFrame::SUBMENU_REINIT);
}

void Func_TogglePad0DAssign()
{
	int i = PADASSIGN_INPUT0D;
	padAssign[i] = (padAssign[i]+1) %4;

	if (padType[i]) Func_AssignPad(i);

	pMenuContext->getFrame(MenuContext::FRAME_CONFIGUREINPUT)->activateSubmenu(ConfigureInputFrame::SUBMENU_REINIT);
}

///////////////////////////////////////////// Second Pad Assign

void Func_TogglePad1AAssign()
{
	int i = PADASSIGN_INPUT1A;
	padAssign[i] = (padAssign[i]+1) %4;

	if (padType[i]) Func_AssignPad(i);

	pMenuContext->getFrame(MenuContext::FRAME_CONFIGUREINPUT)->activateSubmenu(ConfigureInputFrame::SUBMENU_REINIT);
}

void Func_TogglePad1BAssign()
{
	int i = PADASSIGN_INPUT1B;
	padAssign[i] = (padAssign[i]+1) %4;

	if (padType[i]) Func_AssignPad(i);

	pMenuContext->getFrame(MenuContext::FRAME_CONFIGUREINPUT)->activateSubmenu(ConfigureInputFrame::SUBMENU_REINIT);
}

void Func_TogglePad1CAssign()
{
	int i = PADASSIGN_INPUT1C;
	padAssign[i] = (padAssign[i]+1) %4;

	if (padType[i]) Func_AssignPad(i);

	pMenuContext->getFrame(MenuContext::FRAME_CONFIGUREINPUT)->activateSubmenu(ConfigureInputFrame::SUBMENU_REINIT);
}

void Func_TogglePad1DAssign()
{
	int i = PADASSIGN_INPUT1D;
	padAssign[i] = (padAssign[i]+1) %4;

	if (padType[i]) Func_AssignPad(i);

	pMenuContext->getFrame(MenuContext::FRAME_CONFIGUREINPUT)->activateSubmenu(ConfigureInputFrame::SUBMENU_REINIT);
}

