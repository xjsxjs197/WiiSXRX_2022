/**
 * WiiSX - SettingsFrame.cpp
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

#include <sys/stat.h>

#include "MenuContext.h"
#include "SettingsFrame.h"
#include "../libgui/Button.h"
#include "../libgui/TextBox.h"
#include "../libgui/resources.h"
#include "../libgui/FocusManager.h"
#include "../libgui/CursorManager.h"
#include "../libgui/MessageBox.h"
#include "../libgui/gui2/gettext.h"
#include "../wiiSXconfig.h"
#include "../../psxcommon.h"
#include "../vm/vm.h"

#ifdef SHOW_DEBUG
#include "../DEBUG.h"
#endif // SHOW_DEBUG
extern "C" {
#include "../gc_input/controller.h"
#include "../fileBrowser/fileBrowser.h"
#include "../fileBrowser/fileBrowser-libfat.h"
#include "../fileBrowser/fileBrowser-CARD.h"
#include "../../gpu.h"
}

extern void Func_SetPlayGame();

void Func_TabGeneral();
void Func_TabVideo();
void Func_TabInput();
void Func_TabAudio();
void Func_TabSaves();

void Func_CpuInterp();
void Func_CpuDynarec();
void Func_CpuLightrec();
void Func_BiosSelectHLE();
void Func_BiosSelectSD();
void Func_BiosSelectUSB();
void Func_BiosSelectDVD();
void Func_BootBiosYes();
void Func_BootBiosNo();
void Func_ExecuteBios();
void Func_SelectLanguage();
void Func_SaveSettingsSD();
void Func_SaveSettingsUSB();
void Func_SaveSettingsSeparately();

void Func_ShowFpsOn();
void Func_ShowFpsOff();
void Func_FpsLimitAuto();
void Func_FpsLimitOff();
void Func_FrameSkipOn();
void Func_FrameSkipOff();
void Func_ScreenMode();
void Func_Interlaced();
void Func_DeflickerFilter();
void Func_Screen240p();
void Func_BilinearFilter();
void Func_TrapFilter();
void Func_DitheringNone();
void Func_DitheringDefault();
void Func_DitheringAlways();
void Func_ScalingNone();
void Func_Scaling2xSai();
void Func_ForceNTSC();

void Func_ConfigureInput();
void Func_ConfigureButtons();
void Func_PsxTypeStandard();
void Func_PsxTypeAnalog();
void Func_PsxTypeLightgun();
void Func_DisableRumbleYes();
void Func_DisableRumbleNo();
void Func_SaveButtonsSD();
void Func_SaveButtonsUSB();
void Func_SetButtonLoad();
void Func_ToggleButtonLoad();

void Func_DisableAudioYes();
void Func_DisableAudioNo();
void Func_DisableXaYes();
void Func_DisableXaNo();
void Func_DisableCddaYes();
void Func_DisableCddaNo();
void Func_VolumeToggle();

void Func_MemcardSaveSD();
void Func_MemcardSaveUSB();
void Func_MemcardSaveCardA();
void Func_MemcardSaveCardB();
void Func_AutoSaveYes();
void Func_AutoSaveNo();
void Func_SaveStateSD();
void Func_SaveStateUSB();
void Func_Memcard1();
void Func_Memcard2();


void Func_FastloadYes();
void Func_FastloadNo();

void Func_ReturnFromSettingsFrame();
void SetFrameLimit();

extern "C" void VIDEO_SetTrapFilter(bool enable);

extern BOOL hasLoadedISO;
extern int stop;
extern char menuActive;

extern "C" {
void SysReset();
int SysInit();
void SysClose();
void SysStartCPU();
void CheckCdrom();
void CheckPsxType();
void LoadCdrom();
void pauseAudio(void);  void pauseInput(void);
void resumeAudio(void); void resumeInput(void);
void IplFont_loadFontFile(FILE* fontFile);
FILE* IplFont_getFontFile(char* sdUsb);
void psxResetRcntRate();
void pl_chg_psxtype(int is_pal_);
void gpuChangePsxType();
void setSpuInterpolation(int spuInterpolation);
}

#define NUM_FRAME_BUTTONS 64
#define NUM_TAB_BUTTONS 5
#define FRAME_BUTTONS settingsFrameButtons
#define FRAME_STRINGS settingsFrameStrings
#define NUM_FRAME_TEXTBOXES 24
#define FRAME_TEXTBOXES settingsFrameTextBoxes

/*
General Tab:
Select CPU Core: Interpreter; Dynarec
Select Bios: HLE; SD; USB
Boot Games Through Bios: Yes; No
Execute Bios
Save settings: SD; USB

Video Tab:
Show FPS: Yes; No
Limit FPS: Auto; Off; xxx
Frame Skip: On; Off
Screen Mode: 4:3; 16:9
Scaling: None; 2xSaI
Dithering: None; Game Dependent; Always

Input Tab:
Assign Controllers (assign player->pad)
Configure Button Mappings
PSX Controller Type: Standard/Analog/Light Gun
Number of Multitaps: 0, 1, 2

Audio Tab:
Disable Audio: Yes; No
Disable XA: Yes; No
Disable CDDA: Yes; No
Volume Level: ("0: low", "1: medium", "2: loud", "3: loudest")
	Note: iVolume=4-ComboBox_GetCurSel(hWC);, so 1 is loudest and 4 is low; default is medium

Saves Tab:
Memcard Save Device: SD; USB; CardA; CardB
Auto Save Memcards: Yes; No
Save States Device: SD; USB
*/

static char FRAME_STRINGS[79][24] =
	{ "General",
	  "Video",
	  "Input",
	  "Audio",
	  "Saves",
	//Strings for General tab (starting at FRAME_STRINGS[5])
	  "Select CPU Core",
	  "Select Bios",
	  "Boot Through Bios",
	  "Execute Bios",
	  "Save settings",
	  "Interpreter",
	  "Dynarec",
	  "HLE",
	  "SD",
	  "USB",
	  "DVD",
	  "Yes",
	  "No",
	//Strings for Video tab (starting at FRAME_STRINGS[18])..was[])
	  "Show FPS",
	  "Limit FPS",
	  "Frame Skip",
	  "Screen Mode",
	  "Dithering",
	  "Filters",
	  "On",
	  "Off",
	  "Auto",
	  "4:3",
	  "16:9",
	  "Force 16:9",
	  "None",
	  "2xSaI",
	  "Default",
	  "Always",
	//Strings for Input tab (starting at FRAME_STRINGS[34]..was[])
	  "Configure Input",
	  "Configure Buttons",
	  "PSX Controller Type",
	  "Disable Rumble",
	  "Standard",
	  "Analog",
	  "Save Button Configs",
	  "Auto Load Slot:",
	  "Default",
	//Strings for Audio tab (starting at FRAME_STRINGS[43]) ..was[47]
	  "Disable Audio",
	  "Disable XA",
	  "Disable CDDA",
	  "Interpolation",
	  "Simple",	//iVolume=1
	  "Gaussi",
	  "medium",
	  "low",		//iVolume=4
	//Strings for Saves tab (starting at FRAME_STRINGS[51]) ..was[55]
	  "Memcard Save Device",
	  "Auto Save Memcards",
	  "Save States Device",
	  "CardA",
	  "CardB",
      // Strings for display language (starting at FRAME_STRINGS[56]) ..was[62]
      "Select language",
      "En", // English
      "Chs", // Simplified Chinese
      "Kr", // Korean
      "Es", // SPANISH
      "Pte", // PORTUGUESE
      "It", // ITALIAN
      // Strings for display Fast Load (starting at FRAME_STRINGS[63]) ..was[78]
      "Fast Load",
	  "240p",
	  "Bilinear",
	  "Trap",
	  "Interlaced",
	  "Deflicker",
	  "Lightrec",
	  "Lightgun",
	  "GunCon",
	  "Justifier",
	  "Mouse",
	  "Memcard 1",
	  "Memcard 2",
	  "Enable Memcard",
	  "Separately",
	  "Force NTSC"
      };

static char LANG_STRINGS[13][24] =
	{ "En", // English
      "Chs", // Simplified Chinese
      "Kr", // Korean
      "Es", // SPANISH
      "Pte", // PORTUGUESE
      "It", // ITALIAN
      "De", // GERMAN
      "Cht", // TRAD_CHINESE
      "Jp", // JAPANESE
      "Fr", // FRENCH
      "Br", // BRAZILIAN_PORTUGUESE
      "Ca", // CATALAN
      "Tu" // TURKISH
      };

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
	//Buttons for Tabs (starts at button[0])
	{	NULL,	BTN_A_SEL,	FRAME_STRINGS[0],	 25.0,	 30.0,	110.0,	56.0,	-1,	-1,	 4,	 1,	Func_TabGeneral,		Func_ReturnFromSettingsFrame }, // General tab
	{	NULL,	BTN_A_SEL,	FRAME_STRINGS[1],	155.0,	 30.0,	100.0,	56.0,	-1,	-1,	 0,	 2,	Func_TabVideo,			Func_ReturnFromSettingsFrame }, // Video tab
	{	NULL,	BTN_A_SEL,	FRAME_STRINGS[2],	275.0,	 30.0,	100.0,	56.0,	-1,	-1,	 1,	 3,	Func_TabInput,			Func_ReturnFromSettingsFrame }, // Input tab
	{	NULL,	BTN_A_SEL,	FRAME_STRINGS[3],	395.0,	 30.0,	100.0,	56.0,	-1,	-1,	 2,	 4,	Func_TabAudio,			Func_ReturnFromSettingsFrame }, // Audio tab
	{	NULL,	BTN_A_SEL,	FRAME_STRINGS[4],	515.0,	 30.0,	100.0,	56.0,	-1,	-1,	 3,	 0,	Func_TabSaves,			Func_ReturnFromSettingsFrame }, // Saves tab
	//Buttons for General Tab (starts at button[5])
	{	NULL,	BTN_A_SEL,	FRAME_STRINGS[10],	215.0,	100.0,	140.0,	56.0,	 0,	 7,	 58, 6,	Func_CpuInterp,			Func_ReturnFromSettingsFrame }, // CPU: Interp
	{	NULL,	BTN_A_SEL,	FRAME_STRINGS[69],	365.0,	100.0,	130.0,	56.0,	 0,	 9,	 5,	 58,Func_CpuLightrec,		Func_ReturnFromSettingsFrame }, // CPU: Lightrec
	{	NULL,	BTN_A_SEL,	FRAME_STRINGS[12],	295.0,	170.0,	 70.0,	56.0,	 5,	11,	10,	 8,	Func_BiosSelectHLE,		Func_ReturnFromSettingsFrame }, // Bios: HLE
	{	NULL,	BTN_A_SEL,	FRAME_STRINGS[13],	375.0,	170.0,	 55.0,	56.0,	 5,	12,	 7,	 9,	Func_BiosSelectSD,		Func_ReturnFromSettingsFrame }, // Bios: SD
	{	NULL,	BTN_A_SEL,	FRAME_STRINGS[14],	440.0,	170.0,	 70.0,	56.0,	 6,	12,	 8,	10,	Func_BiosSelectUSB,		Func_ReturnFromSettingsFrame }, // Bios: USB
	{	NULL,	BTN_A_SEL,	FRAME_STRINGS[15],	520.0,	170.0,	 70.0,	56.0,	 6,	12,	 9,	 7,	Func_BiosSelectDVD,		Func_ReturnFromSettingsFrame }, // Bios: DVD
	{	NULL,	BTN_A_SEL,	FRAME_STRINGS[16],	295.0,	240.0,	 75.0,	56.0,	 7,	54,	13,	12,	Func_BootBiosYes,		Func_ReturnFromSettingsFrame }, // Boot Thru Bios: Yes
	{	NULL,	BTN_A_SEL,	FRAME_STRINGS[17],	380.0,	240.0,	 75.0,	56.0,	 8,	54,	11,	13,	Func_BootBiosNo,		Func_ReturnFromSettingsFrame }, // Boot Thru Bios: No
	{	NULL,	BTN_A_NRM,	FRAME_STRINGS[8],	465.0,	240.0,	180.0,	56.0,	9,	54,	12,	11,	Func_ExecuteBios,		Func_ReturnFromSettingsFrame }, // Execute Bios

	{	NULL,	BTN_A_NRM,	FRAME_STRINGS[13],	235.0,	380.0,	 55.0,	56.0,	54,	 0,	62,	15,	Func_SaveSettingsSD,	Func_ReturnFromSettingsFrame }, // Save Settings: SD
	{	NULL,	BTN_A_NRM,	FRAME_STRINGS[14],	300.0,	380.0,	 70.0,	56.0,	54,	 0,	14,	62,	Func_SaveSettingsUSB,	Func_ReturnFromSettingsFrame }, // Save Settings: USB

	//Buttons for Video Tab (starts at button[16])
	{	NULL,	BTN_A_SEL,	FRAME_STRINGS[24],	200.0,	100.0,	 75.0,	56.0,	 1,	18,	17,	17,	Func_ShowFpsOn,			Func_ReturnFromSettingsFrame }, // Show FPS: On
	{	NULL,	BTN_A_SEL,	FRAME_STRINGS[25],	295.0,	100.0,	 75.0,	56.0,	 1,	19,	16,	16,	Func_ShowFpsOff,		Func_ReturnFromSettingsFrame }, // Show FPS: Off
	{	NULL,	BTN_A_SEL,	FRAME_STRINGS[26],	200.0,	160.0,	 75.0,	56.0,	16,	20,	63,	19,	Func_FpsLimitAuto,		Func_ReturnFromSettingsFrame }, // FPS Limit: Auto
	{	NULL,	BTN_A_SEL,	FRAME_STRINGS[25],	295.0,	160.0,	 75.0,	56.0,	17,	21,	18,	63,	Func_FpsLimitOff,		Func_ReturnFromSettingsFrame }, // FPS Limit: Off
	{	NULL,	BTN_A_SEL,	FRAME_STRINGS[24],	200.0,	220.0,	 75.0,	56.0,	18,	23,	21,	21,	Func_FrameSkipOn,		Func_ReturnFromSettingsFrame }, // Frame Skip: On
	{	NULL,	BTN_A_SEL,	FRAME_STRINGS[25],	295.0,	220.0,	 75.0,	56.0,	19,	23,	20,	20,	Func_FrameSkipOff,		Func_ReturnFromSettingsFrame }, // Frame Skip: Off
	{	NULL,	BTN_A_NRM,	FRAME_STRINGS[27],	200.0,	280.0,	 135.0,	56.0,	20,	25,	57,	23,	Func_ScreenMode,		Func_ReturnFromSettingsFrame }, // ScreenMode: 4:3/16:9/Force16:9
	{	NULL,	BTN_A_SEL,	FRAME_STRINGS[67],	355.0,	280.0,	 135.0,	56.0,	20,	26,	22,	57,	Func_Interlaced,		Func_ReturnFromSettingsFrame }, // Interlaced Mode
	{	NULL,	BTN_A_SEL,	FRAME_STRINGS[68],	440.0,	400.0,	110.0,	56.0,	27,	 1,	29,	28,	Func_DeflickerFilter,	Func_ReturnFromSettingsFrame }, // Filters: Deflicker Filter
	{	NULL,	BTN_A_SEL,	FRAME_STRINGS[30],	200.0,	340.0,	 75.0,	56.0,	22,	28,	27,	26,	Func_DitheringNone,		Func_ReturnFromSettingsFrame }, // Dithering: None
	{	NULL,	BTN_A_SEL,	FRAME_STRINGS[32],	295.0,	340.0,	110.0,	56.0,	23,	29,	25,	27,	Func_DitheringDefault,	Func_ReturnFromSettingsFrame }, // Dithering: Game Dependent
	{	NULL,	BTN_A_SEL,	FRAME_STRINGS[33],	425.0,	340.0,	110.0,	56.0,	23,	24,	26,	25,	Func_DitheringAlways,	Func_ReturnFromSettingsFrame }, // Dithering: Always
	{	NULL,	BTN_A_SEL,	FRAME_STRINGS[65],	200.0,	400.0,	100.0,	56.0,	25,	 1,	24,	29,	Func_BilinearFilter,	Func_ReturnFromSettingsFrame }, // Filters: Bilinear Filter
	{	NULL,	BTN_A_SEL,	FRAME_STRINGS[66],	320.0,	400.0,	100.0,	56.0,	26,	 1,	28,	24,	Func_TrapFilter,		Func_ReturnFromSettingsFrame }, // Filters: Trap Filter
	//Buttons for Input Tab (starts at button[30])
	{	NULL,	BTN_A_NRM,	FRAME_STRINGS[34],	 90.0,	100.0,	220.0,	56.0,	 2,	32,	31,	31,	Func_ConfigureInput,	Func_ReturnFromSettingsFrame }, // Configure Input Assignment
	{	NULL,	BTN_A_NRM,	FRAME_STRINGS[35],	325.0,	100.0,	235.0,	56.0,	 2,	32,	30,	30,	Func_ConfigureButtons,	Func_ReturnFromSettingsFrame }, // Configure Button Mappings
	{	NULL,	BTN_A_SEL,	FRAME_STRINGS[38],	265.0,	170.0,	115.0,	56.0,	30,	34,	59,	33,	Func_PsxTypeStandard,	Func_ReturnFromSettingsFrame }, // PSX Controller Type: Standard
	{	NULL,	BTN_A_SEL,	FRAME_STRINGS[39],	390.0,	170.0,	110.0,	56.0,	31,	35,	32,	59,	Func_PsxTypeAnalog,		Func_ReturnFromSettingsFrame }, // PSX Controller Type: Analog
	{	NULL,	BTN_A_SEL,	FRAME_STRINGS[16],	285.0,	240.0,	 75.0,	56.0,	32,	36,	35,	35,	Func_DisableRumbleYes,	Func_ReturnFromSettingsFrame }, // Disable Rumble: Yes
	{	NULL,	BTN_A_SEL,	FRAME_STRINGS[17],	380.0,	240.0,	 75.0,	56.0,	33,	37,	34,	34,	Func_DisableRumbleNo,	Func_ReturnFromSettingsFrame }, // Disable Rumble: No
	{	NULL,	BTN_A_NRM,	FRAME_STRINGS[13],	285.0,	310.0,	 55.0,	56.0,	34,	38,	37,	37,	Func_SaveButtonsSD,		Func_ReturnFromSettingsFrame }, // Save Button Mappings: SD
	{	NULL,	BTN_A_NRM,	FRAME_STRINGS[14],	350.0,	310.0,	 70.0,	56.0,	35,	38,	36,	36,	Func_SaveButtonsUSB,	Func_ReturnFromSettingsFrame }, // Save Button Mappings: USB
	{	NULL,	BTN_A_NRM,	FRAME_STRINGS[42],	285.0,	380.0,	135.0,	56.0,	36,	 2,	-1,	-1,	Func_ToggleButtonLoad,	Func_ReturnFromSettingsFrame }, // Auto Load Button Config Slot: Default,1,2,3,4
	//Buttons for Audio Tab (starts at button[39]) ..was[45]
	{	NULL,	BTN_A_SEL,	FRAME_STRINGS[16],	345.0,	100.0,	 75.0,	56.0,	 3,	41,	40,	40,	Func_DisableAudioYes,	Func_ReturnFromSettingsFrame }, // Disable Audio: Yes
	{	NULL,	BTN_A_SEL,	FRAME_STRINGS[17],	440.0,	100.0,	 75.0,	56.0,	 3,	42,	39,	39,	Func_DisableAudioNo,	Func_ReturnFromSettingsFrame }, // Disable Audio: No
	{	NULL,	BTN_A_SEL,	FRAME_STRINGS[16],	345.0,	170.0,	 75.0,	56.0,	39,	43,	42,	42,	Func_DisableXaYes,		Func_ReturnFromSettingsFrame }, // Disable XA: Yes
	{	NULL,	BTN_A_SEL,	FRAME_STRINGS[17],	440.0,	170.0,	 75.0,	56.0,	40,	44,	41,	41,	Func_DisableXaNo,		Func_ReturnFromSettingsFrame }, // Disable XA: No
	{	NULL,	BTN_A_SEL,	FRAME_STRINGS[16],	345.0,	240.0,	 75.0,	56.0,	41,	45,	44,	44,	Func_DisableCddaYes,	Func_ReturnFromSettingsFrame }, // Disable CDDA: Yes
	{	NULL,	BTN_A_SEL,	FRAME_STRINGS[17],	440.0,	240.0,	 75.0,	56.0,	42,	45,	43,	43,	Func_DisableCddaNo,		Func_ReturnFromSettingsFrame }, // Disable CDDA: No
	{	NULL,	BTN_A_NRM,	FRAME_STRINGS[47],	345.0,	310.0,	170.0,	56.0,	43,	 3,	-1,	-1,	Func_VolumeToggle,		Func_ReturnFromSettingsFrame }, // Volume: low/medium/loud/loudest
	//Buttons for Saves Tab (starts at button[46]) ..was[54]
	{	NULL,	BTN_A_SEL,	FRAME_STRINGS[13],	295.0,	100.0,	 55.0,	56.0,	 4,	50,	49,	47,	Func_MemcardSaveSD,		Func_ReturnFromSettingsFrame }, // Memcard Save: SD
	{	NULL,	BTN_A_SEL,	FRAME_STRINGS[14],	360.0,	100.0,	 70.0,	56.0,	 4,	51,	46,	48,	Func_MemcardSaveUSB,	Func_ReturnFromSettingsFrame }, // Memcard Save: USB
	{	NULL,	BTN_A_SEL,	FRAME_STRINGS[54],	440.0,	100.0,	 90.0,	56.0,	 4,	51,	47,	49,	Func_MemcardSaveCardA,	Func_ReturnFromSettingsFrame }, // Memcard Save: Card A
	{	NULL,	BTN_A_SEL,	FRAME_STRINGS[55],	540.0,	100.0,	 90.0,	56.0,	 4,	51,	48,	46,	Func_MemcardSaveCardB,	Func_ReturnFromSettingsFrame }, // Memcard Save: Card B
	{	NULL,	BTN_A_SEL,	FRAME_STRINGS[16],	295.0,	170.0,	 75.0,	56.0,	46,	52,	51,	51,	Func_AutoSaveYes,		Func_ReturnFromSettingsFrame }, // Auto Save Memcards: Yes
	{	NULL,	BTN_A_SEL,	FRAME_STRINGS[17],	380.0,	170.0,	 75.0,	56.0,	47,	53,	50,	50,	Func_AutoSaveNo,		Func_ReturnFromSettingsFrame }, // Auto Save Memcards: No
	{	NULL,	BTN_A_SEL,	FRAME_STRINGS[13],	295.0,	240.0,	 55.0,	56.0,	50,	60,	53,	53,	Func_SaveStateSD,		Func_ReturnFromSettingsFrame }, // Save State: SD
	{	NULL,	BTN_A_SEL,	FRAME_STRINGS[14],	360.0,	240.0,	 70.0,	56.0,	51,	60,	52,	52,	Func_SaveStateUSB,		Func_ReturnFromSettingsFrame }, // Save State: USB
	{	NULL,	BTN_A_NRM,	FRAME_STRINGS[57],	235.0,	310.0,	 90.0,	56.0,	 11,55,	56,	55,	Func_SelectLanguage,	Func_ReturnFromSettingsFrame }, // Select Language: En
    //Buttons for Saves Tab (starts at button[55]) ..was[61]
	{	NULL,	BTN_A_SEL,	FRAME_STRINGS[16],	490.0,	310.0,	 75.0,	56.0,	12,	15,	54,	56,	Func_FastloadYes,		Func_ReturnFromSettingsFrame }, // Fast load: Yes
	{	NULL,	BTN_A_SEL,	FRAME_STRINGS[17],	570.0,	310.0,	 75.0,	56.0,	13,	15,	55,	54,	Func_FastloadNo,		Func_ReturnFromSettingsFrame }, // Fast load: No
	{	NULL,	BTN_A_SEL,	FRAME_STRINGS[64],	510.0,	280.0,	 75.0,	56.0,	21,	27,	23,	22,	Func_Screen240p,		Func_ReturnFromSettingsFrame },  // ScreenMode: 240p
	{	NULL,	BTN_A_SEL,	FRAME_STRINGS[11],	505.0,	100.0,	130.0,	56.0,	 0,	 9,	 6,	 5,	Func_CpuDynarec,		Func_ReturnFromSettingsFrame },  // CPU: Dynarec
	{	NULL,	BTN_A_SEL,	FRAME_STRINGS[70],	510.0,	170.0,	115.0,	56.0,	31,	35,	33,	32,	Func_PsxTypeLightgun,	Func_ReturnFromSettingsFrame },  // PSX Controller Type: Lightgun
	{	NULL,	BTN_A_SEL,	FRAME_STRINGS[74],	295.0,	310.0,	155.0,	56.0,	52,	4,	61,	61,	Func_Memcard1,			Func_ReturnFromSettingsFrame },  // Memcard 1 toggle
	{	NULL,	BTN_A_SEL,	FRAME_STRINGS[75],	460.0,	310.0,	155.0,	56.0,	53,	4,	60,	60,	Func_Memcard2,			Func_ReturnFromSettingsFrame },  // Memcard 2 toggle

	//Buttons for ... (starts at button[62]) ..was[63]
	{	NULL,	BTN_A_NRM,	FRAME_STRINGS[77],	385.0,	380.0,	 140.0,	56.0,	54,	 0,	15,	14,	Func_SaveSettingsSeparately,	Func_ReturnFromSettingsFrame }, // Save Settings: Separately
	{	NULL,	BTN_A_SEL,	FRAME_STRINGS[78],	390.0,	160.0,	160.0,	56.0,	17,	21,	19,	18,	Func_ForceNTSC,			Func_ReturnFromSettingsFrame }  // Force NTSC toggle
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
	//TextBoxes for General Tab (starts at textBox[0])
	{	NULL,	FRAME_STRINGS[5],	105.0,	128.0,	 1.0,	true }, // CPU Core: Pure Interp/Dynarec
	{	NULL,	FRAME_STRINGS[6],	155.0,	198.0,	 1.0,	true }, // Bios: HLE/SD/USB/DVD
	{	NULL,	FRAME_STRINGS[7],	155.0,	268.0,	 1.0,	true }, // Boot Thru Bios: Yes/No
	{	NULL,	FRAME_STRINGS[9],	130.0,	408.0,	 1.0,	true }, // Save settings: SD/USB
	//TextBoxes for Video Tab (starts at textBox[4])
	{	NULL,	FRAME_STRINGS[18],	110.0,	128.0,	 1.0,	true }, // Show FPS: On/Off
	{	NULL,	FRAME_STRINGS[19],	110.0,	188.0,	 1.0,	true }, // Limit FPS: Auto/Off
	{	NULL,	FRAME_STRINGS[20],	110.0,	248.0,	 1.0,	true }, // Frame Skip: On/Off
	{	NULL,	FRAME_STRINGS[21],	90.0,	308.0,	 1.0,	true }, // ScreenMode: 4x3/16x9/Force16x9/Interlaced/240p
	{	NULL,	FRAME_STRINGS[22],	115.0,	368.0,	 1.0,	true }, // Dithering: None/Game Dependent/Always
	{	NULL,	FRAME_STRINGS[23],	110.0,	428.0,	 1.0,	true }, // Filters
	//TextBoxes for Input Tab (starts at textBox[10])
	{	NULL,	FRAME_STRINGS[36],	125.0,	198.0,	 1.0,	true }, // PSX Controller Type: Analog/Digital/Light Gun
	{	NULL,	FRAME_STRINGS[37],	145.0,	268.0,	 1.0,	true }, // Disable Rumble: Yes/No
	{	NULL,	FRAME_STRINGS[40],	145.0,	338.0,	 1.0,	true }, // Save Button Configs: SD/USB
	{	NULL,	FRAME_STRINGS[41],	145.0,	408.0,	 1.0,	true }, // Auto Load Slot: Default/1/2/3/4
	//TextBoxes for Audio Tab (starts at textBox[14]) ..was[17]
	{	NULL,	FRAME_STRINGS[43],	210.0,	128.0,	 1.0,	true }, // Disable Audio: Yes/No
	{	NULL,	FRAME_STRINGS[44],	210.0,	198.0,	 1.0,	true }, // Disable XA Audio: Yes/No
	{	NULL,	FRAME_STRINGS[45],	210.0,	268.0,	 1.0,	true }, // Disable CDDA Audio: Yes/No
	{	NULL,	FRAME_STRINGS[46],	210.0,	338.0,	 1.0,	true }, // Volume: low/medium/loud/loudest
	//TextBoxes for Saves Tab (starts at textBox[18]) ..was[22]
	{	NULL,	FRAME_STRINGS[51],	150.0,	128.0,	 1.0,	true }, // Memcard Save Device: SD/USB/CardA/CardB
	{	NULL,	FRAME_STRINGS[52],	150.0,	198.0,	 1.0,	true }, // Auto Save Memcards: Yes/No
	{	NULL,	FRAME_STRINGS[53],	150.0,	268.0,	 1.0,	true }, // Save State Device: SD/USB
	{	NULL,	FRAME_STRINGS[56],	130.0,	338.0,	 1.0,	true }, // Select language: En, Chs, ......
	{	NULL,	FRAME_STRINGS[63],	405.0,	338.0,	 1.0,	true }, // Fast load
	{	NULL,	FRAME_STRINGS[76],	150.0,	338.0,	 1.0,	true }, // Memcard enable
};

SettingsFrame::SettingsFrame()
		: activeSubmenu(SUBMENU_GENERAL)
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
	setBackFunc(Func_ReturnFromSettingsFrame);
	setEnabled(true);
	activateSubmenu(SUBMENU_GENERAL);
}

SettingsFrame::~SettingsFrame()
{
	for (int i = 0; i < NUM_FRAME_TEXTBOXES; i++)
		delete FRAME_TEXTBOXES[i].textBox;
	for (int i = 0; i < NUM_FRAME_BUTTONS; i++)
	{
		menu::Cursor::getInstance().removeComponent(this, FRAME_BUTTONS[i].button);
		delete FRAME_BUTTONS[i].button;
	}
}

extern char fastLoad; // variable for see if game has reduce load time

void SettingsFrame::activateSubmenu(int submenu)
{
	activeSubmenu = submenu;

	//All buttons: hide; unselect
	for (int i = 0; i < NUM_FRAME_BUTTONS; i++)
	{
		FRAME_BUTTONS[i].button->setVisible(false);
		FRAME_BUTTONS[i].button->setSelected(false);
		FRAME_BUTTONS[i].button->setActive(false);
	}
	//All textBoxes: hide
	for (int i = 0; i < NUM_FRAME_TEXTBOXES; i++)
	{
		FRAME_TEXTBOXES[i].textBox->setVisible(false);
	}
	switch (activeSubmenu)	//Tab buttons: set visible; set focus up/down; set selected
	{						//Config buttons: set visible; set selected
		case SUBMENU_GENERAL:
			setDefaultFocus(FRAME_BUTTONS[0].button);
			for (int i = 0; i < NUM_TAB_BUTTONS; i++)
			{
				FRAME_BUTTONS[i].button->setVisible(true);
				FRAME_BUTTONS[i].button->setNextFocus(menu::Focus::DIRECTION_DOWN, FRAME_BUTTONS[5].button);
				FRAME_BUTTONS[i].button->setNextFocus(menu::Focus::DIRECTION_UP, FRAME_BUTTONS[14].button);
				FRAME_BUTTONS[i].button->setActive(true);
			}
			for (int i = 0; i < 4; i++)
				FRAME_TEXTBOXES[i].textBox->setVisible(true);

            FRAME_TEXTBOXES[21].textBox->setVisible(true);
            FRAME_TEXTBOXES[22].textBox->setVisible(true);
			FRAME_BUTTONS[0].button->setSelected(true);
			if (dynacore == DYNACORE_INTERPRETER)
			{
				FRAME_BUTTONS[5].button->setSelected(true);
			}
			else if (dynacore == DYNACORE_DYNAREC)
			{
				FRAME_BUTTONS[6].button->setSelected(true);
			}
			else
			{
				FRAME_BUTTONS[58].button->setSelected(true);
			}

			FRAME_BUTTONS[7+biosDevice].button->setSelected(true);
			if (LoadCdBios == BOOTTHRUBIOS_YES)	FRAME_BUTTONS[11].button->setSelected(true);
			else								FRAME_BUTTONS[12].button->setSelected(true);
			for (int i = 5; i < 16; i++)
			{
				FRAME_BUTTONS[i].button->setVisible(true);
				FRAME_BUTTONS[i].button->setActive(true);
			}
			FRAME_BUTTONS[54].button->setVisible(true);
			FRAME_BUTTONS[54].button->setActive(true);
            FRAME_BUTTONS[54].buttonString = LANG_STRINGS[lang];

            // Fast load
            FRAME_BUTTONS[55].button->setVisible(true);
            FRAME_BUTTONS[56].button->setVisible(true);
            FRAME_BUTTONS[55].button->setActive(true);
            FRAME_BUTTONS[56].button->setActive(true);
            FRAME_BUTTONS[55].button->setSelected(fastLoad == 1);
            FRAME_BUTTONS[56].button->setSelected(fastLoad == 0);

            // CPU: Dynarec
            FRAME_BUTTONS[58].button->setVisible(true);
            FRAME_BUTTONS[58].button->setActive(true);

            // Save Settings: Separately
            FRAME_BUTTONS[62].button->setVisible(true);
            FRAME_BUTTONS[62].button->setActive(hasLoadedISO ? true : false);
			break;
		case SUBMENU_VIDEO:
			setDefaultFocus(FRAME_BUTTONS[1].button);
			for (int i = 0; i < NUM_TAB_BUTTONS; i++)
			{
				FRAME_BUTTONS[i].button->setVisible(true);
				FRAME_BUTTONS[i].button->setNextFocus(menu::Focus::DIRECTION_DOWN, FRAME_BUTTONS[16].button);
				FRAME_BUTTONS[i].button->setNextFocus(menu::Focus::DIRECTION_UP, FRAME_BUTTONS[26].button);
				FRAME_BUTTONS[i].button->setActive(true);
			}
			for (int i = 4; i < 10; i++)
				FRAME_TEXTBOXES[i].textBox->setVisible(true);
			FRAME_BUTTONS[1].button->setSelected(true);
			if (showFPSonScreen == FPS_SHOW)	FRAME_BUTTONS[16].button->setSelected(true);
			else								FRAME_BUTTONS[17].button->setSelected(true);
			if (frameLimit[1] == FRAMELIMIT_AUTO)	FRAME_BUTTONS[18].button->setSelected(true);
			else								FRAME_BUTTONS[19].button->setSelected(true);
			if (frameSkip == FRAMESKIP_ENABLE)	FRAME_BUTTONS[20].button->setSelected(true);
			else								FRAME_BUTTONS[21].button->setSelected(true);
			if (originalMode == ORIGINALMODE_ENABLE)FRAME_BUTTONS[57].button->setSelected(true);
			if (bilinearFilter == BILINEARFILTER_ENABLE)FRAME_BUTTONS[28].button->setSelected(true);
			if (trapFilter == TRAPFILTER_ENABLE)FRAME_BUTTONS[29].button->setSelected(true);
			if (interlacedMode == INTERLACED_ENABLE)FRAME_BUTTONS[23].button->setSelected(true);
			if (deflickerFilter == DEFLICKER_ENABLE)FRAME_BUTTONS[24].button->setSelected(true);
			FRAME_BUTTONS[22].buttonString = FRAME_STRINGS[27 + screenMode];

			FRAME_BUTTONS[57].button->setVisible(true);
			FRAME_BUTTONS[57].button->setActive(true);

			FRAME_BUTTONS[25+useDithering].button->setSelected(true);
			for (int i = 16; i < 30; i++)
			{
				FRAME_BUTTONS[i].button->setVisible(true);
				FRAME_BUTTONS[i].button->setActive(true);
			}

			// force NTSC
            if (forceNTSC == FORCENTSC_ENABLE)FRAME_BUTTONS[63].button->setSelected(true);
            FRAME_BUTTONS[63].button->setVisible(true);
			FRAME_BUTTONS[63].button->setActive(true);

			break;
		case SUBMENU_INPUT:
			Func_SetButtonLoad();
			setDefaultFocus(FRAME_BUTTONS[2].button);
			for (int i = 0; i < NUM_TAB_BUTTONS; i++)
			{
				FRAME_BUTTONS[i].button->setVisible(true);
				FRAME_BUTTONS[i].button->setNextFocus(menu::Focus::DIRECTION_DOWN, FRAME_BUTTONS[30].button);
				FRAME_BUTTONS[i].button->setNextFocus(menu::Focus::DIRECTION_UP, FRAME_BUTTONS[38].button);
				FRAME_BUTTONS[i].button->setActive(true);
			}
			for (int i = 10; i < 14; i++)
				FRAME_TEXTBOXES[i].textBox->setVisible(true);
			FRAME_BUTTONS[2].button->setSelected(true);
			if (controllerType == CONTROLLERTYPE_STANDARD)FRAME_BUTTONS[32].button->setSelected(true);
			if (controllerType == CONTROLLERTYPE_ANALOG)FRAME_BUTTONS[33].button->setSelected(true);
			if (lightGun != LIGHTGUN_DISABLE)FRAME_BUTTONS[59].button->setSelected(true);
			else FRAME_BUTTONS[59].button->setSelected(false);
			FRAME_BUTTONS[59].buttonString = FRAME_STRINGS[70 + lightGun];

			FRAME_BUTTONS[34+rumbleEnabled].button->setSelected(true);

			FRAME_BUTTONS[59].button->setVisible(true);
			FRAME_BUTTONS[59].button->setActive(true);

			for (int i = 30; i < 39; i++)
			{
				FRAME_BUTTONS[i].button->setVisible(true);
				FRAME_BUTTONS[i].button->setActive(true);
			}
			break;
		case SUBMENU_AUDIO:
			setDefaultFocus(FRAME_BUTTONS[3].button);
			for (int i = 0; i < NUM_TAB_BUTTONS; i++)
			{
				FRAME_BUTTONS[i].button->setVisible(true);
				FRAME_BUTTONS[i].button->setNextFocus(menu::Focus::DIRECTION_DOWN, FRAME_BUTTONS[39].button);
				FRAME_BUTTONS[i].button->setNextFocus(menu::Focus::DIRECTION_UP, FRAME_BUTTONS[45].button);
				FRAME_BUTTONS[i].button->setActive(true);
			}
			for (int i = 14; i < 18; i++)
				FRAME_TEXTBOXES[i].textBox->setVisible(true);
			FRAME_BUTTONS[3].button->setSelected(true);
			if (audioEnabled == AUDIO_DISABLE)	FRAME_BUTTONS[39].button->setSelected(true);
			else								FRAME_BUTTONS[40].button->setSelected(true);
			if (Config.Xa == XA_DISABLE)		FRAME_BUTTONS[41].button->setSelected(true);
			else								FRAME_BUTTONS[42].button->setSelected(true);
			if (Config.Cdda == CDDA_DISABLE)	FRAME_BUTTONS[43].button->setSelected(true);
			else								FRAME_BUTTONS[44].button->setSelected(true);
			FRAME_BUTTONS[45].buttonString = FRAME_STRINGS[46 + spuInterpolation];
			for (int i = 39; i < 46; i++)
			{
				FRAME_BUTTONS[i].button->setVisible(true);
				FRAME_BUTTONS[i].button->setActive(true);
			}
			break;
		case SUBMENU_SAVES:
			setDefaultFocus(FRAME_BUTTONS[4].button);
			for (int i = 0; i < NUM_TAB_BUTTONS; i++)
			{
				FRAME_BUTTONS[i].button->setVisible(true);
				FRAME_BUTTONS[i].button->setNextFocus(menu::Focus::DIRECTION_DOWN, FRAME_BUTTONS[46].button);
				FRAME_BUTTONS[i].button->setNextFocus(menu::Focus::DIRECTION_UP, FRAME_BUTTONS[52].button);
				FRAME_BUTTONS[i].button->setActive(true);
			}
			for (int i = 18; i < 21; i++)
				FRAME_TEXTBOXES[i].textBox->setVisible(true);
			FRAME_TEXTBOXES[23].textBox->setVisible(true);
			FRAME_BUTTONS[4].button->setSelected(true);
			FRAME_BUTTONS[46+nativeSaveDevice].button->setSelected(true);
			if (autoSave == AUTOSAVE_ENABLE)	FRAME_BUTTONS[50].button->setSelected(true);
			else								FRAME_BUTTONS[51].button->setSelected(true);
			if (saveStateDevice == SAVESTATEDEVICE_SD)	FRAME_BUTTONS[52].button->setSelected(true);
			else										FRAME_BUTTONS[53].button->setSelected(true);
			if (memCard[0] == MEMCARD_ENABLE)FRAME_BUTTONS[60].button->setSelected(true);
			if (memCard[1] == MEMCARD_ENABLE)FRAME_BUTTONS[61].button->setSelected(true);
			FRAME_BUTTONS[60].button->setVisible(true);
			FRAME_BUTTONS[60].button->setActive(true);
			FRAME_BUTTONS[61].button->setVisible(true);
			FRAME_BUTTONS[61].button->setActive(true);
			for (int i = 46; i < NUM_FRAME_BUTTONS; i++)
			{
			    if (i >= 54) {
                    // language info
                    continue;
			    }
				FRAME_BUTTONS[i].button->setVisible(true);
				FRAME_BUTTONS[i].button->setActive(true);
			}
			break;
	}
}

void SettingsFrame::drawChildren(menu::Graphics &gfx)
{
	if(isVisible())
	{
#ifdef HW_RVL
		WPADData* wiiPad = menu::Input::getInstance().getWpad();
#endif
		for (int i=0; i<4; i++)
		{
			u16 currentButtonsGC = PAD_ButtonsHeld(i);
			if (currentButtonsGC ^ previousButtonsGC[i])
			{
				u16 currentButtonsDownGC = (currentButtonsGC ^ previousButtonsGC[i]) & currentButtonsGC;
				previousButtonsGC[i] = currentButtonsGC;
				if (currentButtonsDownGC & PAD_TRIGGER_R)
				{
					//move to next tab
					if(activeSubmenu < SUBMENU_SAVES)
					{
						activateSubmenu(activeSubmenu+1);
						menu::Focus::getInstance().clearPrimaryFocus();
					}
					break;
				}
				else if (currentButtonsDownGC & PAD_TRIGGER_L)
				{
					//move to the previous tab
					if(activeSubmenu > SUBMENU_GENERAL)
					{
						activateSubmenu(activeSubmenu-1);
						menu::Focus::getInstance().clearPrimaryFocus();
					}
					break;
				}
			}
#ifdef HW_RVL
			else if (wiiPad[i].btns_h ^ previousButtonsWii[i])
			{
				u32 currentButtonsDownWii = (wiiPad[i].btns_h ^ previousButtonsWii[i]) & wiiPad[i].btns_h;
				previousButtonsWii[i] = wiiPad[i].btns_h;
				if (wiiPad[i].exp.type == WPAD_EXP_CLASSIC)
				{
					if (currentButtonsDownWii & WPAD_CLASSIC_BUTTON_FULL_R)
					{
						//move to next tab
						if(activeSubmenu < SUBMENU_SAVES)
						{
							activateSubmenu(activeSubmenu+1);
							menu::Focus::getInstance().clearPrimaryFocus();
						}
						break;
					}
					else if (currentButtonsDownWii & WPAD_CLASSIC_BUTTON_FULL_L)
					{
						//move to the previous tab
						if(activeSubmenu > SUBMENU_GENERAL)
						{
							activateSubmenu(activeSubmenu-1);
							menu::Focus::getInstance().clearPrimaryFocus();
						}
						break;
					}
				}
				else
				{
					if (currentButtonsDownWii & WPAD_BUTTON_PLUS)
					{
						//move to next tab
						if(activeSubmenu < SUBMENU_SAVES)
						{
							activateSubmenu(activeSubmenu+1);
							menu::Focus::getInstance().clearPrimaryFocus();
						}
						break;
					}
					else if (currentButtonsDownWii & WPAD_BUTTON_MINUS)
					{
						//move to the previous tab
						if(activeSubmenu > SUBMENU_GENERAL)
						{
							activateSubmenu(activeSubmenu-1);
							menu::Focus::getInstance().clearPrimaryFocus();
						}
						break;
					}
				}
			}
			else if (WUPC_ButtonsHeld(i) ^ previousButtonsWii[i])
			{
				u32 wupcHeld = WUPC_ButtonsHeld(i);
				u32 currentButtonsDownWii = (wupcHeld ^ previousButtonsWii[i]) & wupcHeld;
				previousButtonsWii[i] = wupcHeld;

				if (currentButtonsDownWii & WPAD_CLASSIC_BUTTON_FULL_R)
				{
					//move to next tab
					if (activeSubmenu < SUBMENU_SAVES)
					{
						activateSubmenu(activeSubmenu + 1);
						menu::Focus::getInstance().clearPrimaryFocus();
					}
					break;
				}
				else if (currentButtonsDownWii & WPAD_CLASSIC_BUTTON_FULL_L)
				{
					//move to the previous tab
					if (activeSubmenu > SUBMENU_GENERAL)
					{
						activateSubmenu(activeSubmenu - 1);
						menu::Focus::getInstance().clearPrimaryFocus();
					}
					break;
				}
			}
			else if (i == 0 && WiiDRC_ButtonsHeld() ^ previousButtonsWii[i])
			{
				u16 wiidrcHeld = WiiDRC_ButtonsHeld();
				u16 currentButtonsDownWii = (wiidrcHeld ^ previousButtonsWii[i]) & wiidrcHeld;
				previousButtonsWii[i] = wiidrcHeld;

				if (currentButtonsDownWii & WIIDRC_BUTTON_ZL)
				{
					//move to next tab
					if (activeSubmenu < SUBMENU_SAVES)
					{
						activateSubmenu(activeSubmenu + 1);
						menu::Focus::getInstance().clearPrimaryFocus();
					}
					break;
				}
				else if (currentButtonsDownWii & WIIDRC_BUTTON_ZR)
				{
					//move to the previous tab
					if (activeSubmenu > SUBMENU_GENERAL)
					{
						activateSubmenu(activeSubmenu - 1);
						menu::Focus::getInstance().clearPrimaryFocus();
					}
					break;
				}
			}
#endif //HW_RVL
		}

		//Draw buttons
		menu::ComponentList::const_iterator iteration;
		for (iteration = componentList.begin(); iteration != componentList.end(); ++iteration)
		{
			(*iteration)->draw(gfx);
		}
	}
}

extern MenuContext *pMenuContext;

void Func_TabGeneral()
{
	pMenuContext->setActiveFrame(MenuContext::FRAME_SETTINGS,SettingsFrame::SUBMENU_GENERAL);
}

void Func_TabVideo()
{
	pMenuContext->setActiveFrame(MenuContext::FRAME_SETTINGS,SettingsFrame::SUBMENU_VIDEO);
}

void Func_TabInput()
{
	pMenuContext->setActiveFrame(MenuContext::FRAME_SETTINGS,SettingsFrame::SUBMENU_INPUT);
}

void Func_TabAudio()
{
	pMenuContext->setActiveFrame(MenuContext::FRAME_SETTINGS,SettingsFrame::SUBMENU_AUDIO);
}

void Func_TabSaves()
{
	pMenuContext->setActiveFrame(MenuContext::FRAME_SETTINGS,SettingsFrame::SUBMENU_SAVES);
}

#define ChangeCpu(cpuCore, btnIdx) \
    FRAME_BUTTONS[5].button->setSelected(false); \
    FRAME_BUTTONS[6].button->setSelected(false); \
    FRAME_BUTTONS[58].button->setSelected(false); \
    FRAME_BUTTONS[btnIdx].button->setSelected(true); \
    int needInit = 0; \
	if (hasLoadedISO && dynacore != cpuCore) { \
		SysClose(); \
		needInit = 1; \
	} \
	dynacore = cpuCore; \
	if(hasLoadedISO && needInit) { \
		if (dynacore == DYNACORE_DYNAREC) \
		{ \
			VM_Init(1024*1024, 256*1024); \
		} \
		SysInit(); \
		CheckCdrom(); \
		SysReset(); \
		LoadCdrom(); \
		Func_SetPlayGame(); \
		menu::MessageBox::getInstance().setMessage("Game Reset"); \
	}

void Func_CpuInterp()
{
	ChangeCpu(DYNACORE_INTERPRETER, 5);
}

void Func_CpuDynarec()
{
	ChangeCpu(DYNACORE_DYNAREC_OLD, 58);
}

void Func_CpuLightrec()
{
	ChangeCpu(DYNACORE_DYNAREC, 6);
}

void Func_BiosSelectHLE()
{
	for (int i = 7; i <= 10; i++)
		FRAME_BUTTONS[i].button->setSelected(false);
	FRAME_BUTTONS[7].button->setSelected(true);

	int needInit = 0;
	if(hasLoadedISO && biosDevice != BIOSDEVICE_HLE){ SysClose(); needInit = 1; }
	biosDevice = BIOSDEVICE_HLE;
	if(hasLoadedISO && needInit) {
		SysInit ();
		CheckCdrom();
		SysReset();
		LoadCdrom();
		Func_SetPlayGame();
		menu::MessageBox::getInstance().setMessage("Game Reset");
	}
}

int checkBiosExists(int testDevice)
{
	fileBrowser_file testFile;
	memset(&testFile, 0, sizeof(fileBrowser_file));

	biosFile_dir = (testDevice == BIOSDEVICE_SD) ? &biosDir_libfat_Default : &biosDir_libfat_USB;
	sprintf(&testFile.name[0], "%s/SCPH1001.BIN", &biosFile_dir->name[0]);
	biosFile_readFile  = fileBrowser_libfat_readFile;
	biosFile_open      = fileBrowser_libfat_open;
	biosFile_init      = fileBrowser_libfat_init;
	biosFile_deinit    = fileBrowser_libfat_deinit;
	biosFile_init(&testFile);  //initialize the bios device (it might not be the same as ISO device)
	return biosFile_open(&testFile);
}

void Func_BiosSelectSD()
{
	for (int i = 7; i <= 10; i++)
		FRAME_BUTTONS[i].button->setSelected(false);

	int needInit = 0;
	if(checkBiosExists(BIOSDEVICE_SD) == FILE_BROWSER_ERROR_NO_FILE) {
		menu::MessageBox::getInstance().setMessage("BIOS not found on SD");
		if(hasLoadedISO && biosDevice != BIOSDEVICE_HLE){ SysClose(); needInit = 1; }
		biosDevice = BIOSDEVICE_HLE;
		FRAME_BUTTONS[7].button->setSelected(true);
	}
	else {
		if(hasLoadedISO && biosDevice != BIOSDEVICE_SD){ SysClose(); needInit = 1; }
		biosDevice = BIOSDEVICE_SD;
		FRAME_BUTTONS[8].button->setSelected(true);
	}
	if(hasLoadedISO && needInit) {
		SysInit ();
		CheckCdrom();
		SysReset();
		LoadCdrom();
		Func_SetPlayGame();
		menu::MessageBox::getInstance().setMessage("Game Reset");
	}
}

void Func_BiosSelectUSB()
{
	for (int i = 7; i <= 10; i++)
		FRAME_BUTTONS[i].button->setSelected(false);

	int needInit = 0;
	if(checkBiosExists(BIOSDEVICE_USB) == FILE_BROWSER_ERROR_NO_FILE) {
		menu::MessageBox::getInstance().setMessage("BIOS not found on USB");
		if(hasLoadedISO && biosDevice != BIOSDEVICE_HLE){ SysClose(); needInit = 1; }
		biosDevice = BIOSDEVICE_HLE;
		FRAME_BUTTONS[7].button->setSelected(true);
	}
	else {
		if(hasLoadedISO && biosDevice != BIOSDEVICE_USB){ SysClose(); needInit = 1; }
		biosDevice = BIOSDEVICE_USB;
		FRAME_BUTTONS[9].button->setSelected(true);
	}
	if(hasLoadedISO && needInit) {
		SysInit ();
		CheckCdrom();
		SysReset();
		LoadCdrom();
		Func_SetPlayGame();
		menu::MessageBox::getInstance().setMessage("Game Reset");
	}
}

void Func_BiosSelectDVD()
{
  	menu::MessageBox::getInstance().setMessage("DVD BIOS not implemented");
}

void Func_BootBiosYes()
{
	/* If HLE bios selected, boot thru bios shouldn't make a difference. TODO: Check this.
	if(biosDevice == BIOSDEVICE_HLE) {
		menu::MessageBox::getInstance().setMessage("You must select a BIOS, not HLE");
		return;
	}*/

	for (int i = 11; i <= 12; i++)
		FRAME_BUTTONS[i].button->setSelected(false);
	FRAME_BUTTONS[11].button->setSelected(true);

	LoadCdBios = BOOTTHRUBIOS_YES;
}

void Func_BootBiosNo()
{
	for (int i = 11; i <= 12; i++)
		FRAME_BUTTONS[i].button->setSelected(false);
	FRAME_BUTTONS[12].button->setSelected(true);

	LoadCdBios = BOOTTHRUBIOS_NO;
}

bool executedBios = false;
bool executingBios = false;
void biosFileInit();

void Func_ExecuteBios()
{
	if (!hasLoadedISO)
	{
		frameLimit[0] = FRAMELIMIT_AUTO;
	    frameLimit[1] = FRAMELIMIT_AUTO;
		biosFileInit();
	}
	LoadCdBios = BOOTTHRUBIOS_YES;
	if(Config.HLE == BIOS_HLE) {
		menu::MessageBox::getInstance().setMessage("You must select a BIOS, not HLE");
		return;
	}
	if(hasLoadedISO || executedBios) {
		//TODO: Implement yes/no that current game will be reset
		SysClose();
	}

	//TODO: load/save memcards here
	if(SysInit() < 0) {
		menu::MessageBox::getInstance().setMessage("Failed to initialize system.\nTry loading an ISO.");
		return;
	}
	executedBios = true;
	executingBios = true;
	CheckCdrom();
	SysReset();
	pauseRemovalThread();
	resumeAudio();
	resumeInput();
	menuActive = 0;
	SysStartCPU();
	menuActive = 1;
	pauseInput();
	pauseAudio();
	continueRemovalThread();
}

void Func_SelectLanguage()
{
    FILE* fontFile = NULL;
    do
	{
		lang++;
		if (lang > TURKISH)
		{
			lang = ENGLISH;
		}
		else
		{
			fontFile = IplFont_getFontFile("sd");
			if (fontFile == NULL)
			{
				fontFile = IplFont_getFontFile("usb");
			}
		}
	}
	while (lang != ENGLISH && fontFile == NULL);

    IplFont_loadFontFile(fontFile);
    ChangeLanguage();

    FRAME_BUTTONS[54].button->setSelected(true);
    FRAME_BUTTONS[54].buttonString = LANG_STRINGS[lang];
}

extern void writeConfig(FILE* f);

void Func_SaveSettingsSD()
{
	fileBrowser_file* configFile_file;
	int (*configFile_init)(fileBrowser_file*) = fileBrowser_libfat_init;
	configFile_file = &saveDir_libfat_Default;
	struct stat s;
	if (stat("sd:/wiisxrx/", &s)) {
		menu::MessageBox::getInstance().setMessage("Error opening directory sd:/wiisxrx");
		return;
	}
	if(configFile_init(configFile_file)) {                //only if device initialized ok
		FILE* f = fopen( "sd:/wiisxrx/settingsRX2022.cfg", "wb" );  //attempt to open file
		if(f) {
			writeConfig(f);                                   //write out the config
			fclose(f);
			menu::MessageBox::getInstance().setMessage("Saved settings to SD");
			return;
		}
	}
	menu::MessageBox::getInstance().setMessage("Error saving settings to SD");
}

void Func_SaveSettingsUSB()
{
	fileBrowser_file* configFile_file;
	int (*configFile_init)(fileBrowser_file*) = fileBrowser_libfat_init;
	configFile_file = &saveDir_libfat_USB;
	struct stat s;
	if (stat("usb:/wiisxrx/", &s)) {
		menu::MessageBox::getInstance().setMessage("Error opening directory usb:/wiisxrx");
		return;
	}
	if(configFile_init(configFile_file)) {                //only if device initialized ok
		FILE* f = fopen( "usb:/wiisxrx/settingsRX2022.cfg", "wb" ); //attempt to open file
		if(f) {
			writeConfig(f);                                   //write out the config
			fclose(f);
			menu::MessageBox::getInstance().setMessage("Saved settings to USB");
			return;
		}
	}
	menu::MessageBox::getInstance().setMessage("Error saving settings to USB");
}

void Func_SaveSettingsSeparately()
{
    struct stat s;
    char settingPathBuf[256];
    fileBrowser_file* configFile_file;
    extern char CdromId[10];
	if (stat("usb:/wiisxrx/settings/", &s)) {
		if (stat("sd:/wiisxrx/settings/", &s)) {
			menu::MessageBox::getInstance().setMessage("Error opening directory sd:/wiisxrx/settings/");
			return;
		}
		else
		{
			sprintf(settingPathBuf, "%s%s%s", "sd:/wiisxrx/settings/", CdromId, ".cfg");
			configFile_file = &saveDir_libfat_Default;
	        int (*configFile_init)(fileBrowser_file*) = fileBrowser_libfat_init;
	        if(configFile_init(configFile_file)) {                //only if device initialized ok
				FILE* f = fopen( settingPathBuf, "wb" );  //attempt to open file
				if(f) {
					writeConfig(f);                                   //write out the config
					fclose(f);
					menu::MessageBox::getInstance().setMessage("Saved settings to SD");
					return;
				}
			}
			menu::MessageBox::getInstance().setMessage("Error saving settings to SD");
		}
	}
	else
	{
		sprintf(settingPathBuf, "%s%s%s", "usb:/wiisxrx/settings/", CdromId, ".cfg");
		configFile_file = &saveDir_libfat_USB;
	    int (*configFile_init)(fileBrowser_file*) = fileBrowser_libfat_init;
	    if (configFile_init(configFile_file)) {                //only if device initialized ok
			FILE* f = fopen( settingPathBuf, "wb" ); //attempt to open file
			if(f) {
				writeConfig(f);                                   //write out the config
				fclose(f);
				menu::MessageBox::getInstance().setMessage("Saved settings to USB");
				return;
			}
		}
		menu::MessageBox::getInstance().setMessage("Error saving settings to USB");
	}
}
void Func_ShowFpsOn()
{
	for (int i = 16; i <= 17; i++)
		FRAME_BUTTONS[i].button->setSelected(false);
	FRAME_BUTTONS[16].button->setSelected(true);
	showFPSonScreen = FPS_SHOW;
}

void Func_ShowFpsOff()
{
	for (int i = 16; i <= 17; i++)
		FRAME_BUTTONS[i].button->setSelected(false);
	FRAME_BUTTONS[17].button->setSelected(true);
	showFPSonScreen = FPS_HIDE;
}

void SetFrameLimit()
{
    if (gpuPtr == &oldSoftGpu)
    {
        gpuPtr->setframelimit(0);
    }
}

void Func_FpsLimitAuto()
{
	for (int i = 18; i <= 19; i++)
		FRAME_BUTTONS[i].button->setSelected(false);
	FRAME_BUTTONS[18].button->setSelected(true);
	frameLimit[0] = FRAMELIMIT_AUTO;
	frameLimit[1] = FRAMELIMIT_AUTO;
	SetFrameLimit();
}

void Func_FpsLimitOff()
{
	for (int i = 18; i <= 19; i++)
		FRAME_BUTTONS[i].button->setSelected(false);
	FRAME_BUTTONS[19].button->setSelected(true);
	frameLimit[0] = FRAMELIMIT_NONE;
	frameLimit[1] = FRAMELIMIT_NONE;
	SetFrameLimit();
}

void Func_FrameSkipOn()
{
	for (int i = 20; i <= 21; i++)
		FRAME_BUTTONS[i].button->setSelected(false);
	FRAME_BUTTONS[20].button->setSelected(true);
	frameSkip = FRAMESKIP_ENABLE;
	SetFrameLimit();
}

void Func_FrameSkipOff()
{
	for (int i = 20; i <= 21; i++)
		FRAME_BUTTONS[i].button->setSelected(false);
	FRAME_BUTTONS[21].button->setSelected(true);
	frameSkip = FRAMESKIP_DISABLE;
	SetFrameLimit();
}


void Func_Interlaced()
{
	if(interlacedMode == INTERLACED_ENABLE)
	{
		FRAME_BUTTONS[23].button->setSelected(false);
		interlacedMode = INTERLACED_DISABLE;
	}
	else
	{
		FRAME_BUTTONS[23].button->setSelected(true);
		interlacedMode = INTERLACED_ENABLE;
	}
}


void Func_DeflickerFilter()
{
	if(deflickerFilter == DEFLICKER_ENABLE)
	{
		FRAME_BUTTONS[24].button->setSelected(false);
		deflickerFilter = DEFLICKER_DISABLE;
	}
	else
	{
		FRAME_BUTTONS[24].button->setSelected(true);
		deflickerFilter = DEFLICKER_ENABLE;
	}
}

void Func_ScreenMode()
{
	//SCREENMODE_4x3=0,
	//SCREENMODE_16x9,
	//SCREENMODE_16x9_PILLARBOX
    screenMode++;
    if (screenMode > SCREENMODE_16x9_PILLARBOX)
    {
        screenMode = SCREENMODE_4x3;
    }
    FRAME_BUTTONS[22].button->setSelected(true);
    FRAME_BUTTONS[22].buttonString = FRAME_STRINGS[27 + screenMode];
}

void Func_Screen240p()
{
	if(originalMode == ORIGINALMODE_ENABLE)
	{
		FRAME_BUTTONS[57].button->setSelected(false);
		originalMode = ORIGINALMODE_DISABLE;
	}
	else
	{
		FRAME_BUTTONS[57].button->setSelected(true);
		originalMode = ORIGINALMODE_ENABLE;
	}
}

void Func_DitheringNone()
{
	for (int i = 25; i <= 27; i++)
		FRAME_BUTTONS[i].button->setSelected(false);
	FRAME_BUTTONS[25].button->setSelected(true);
	useDithering = USEDITHER_NONE;
	// Setting variables directly in the GPU is not good......
    iUseDither = useDithering;
}

void Func_DitheringDefault()
{
	for (int i = 25; i <= 27; i++)
		FRAME_BUTTONS[i].button->setSelected(false);
	FRAME_BUTTONS[26].button->setSelected(true);
	useDithering = USEDITHER_DEFAULT;
	// Setting variables directly in the GPU is not good......
    iUseDither = useDithering;
}

void Func_DitheringAlways()
{
	for (int i = 25; i <= 27; i++)
		FRAME_BUTTONS[i].button->setSelected(false);
	FRAME_BUTTONS[27].button->setSelected(true);
	useDithering = USEDITHER_ALWAYS;
	// Setting variables directly in the GPU is not good......
    iUseDither = useDithering;
}

void Func_BilinearFilter()
{
	if(bilinearFilter == BILINEARFILTER_ENABLE)
	{
		FRAME_BUTTONS[28].button->setSelected(false);
		bilinearFilter = BILINEARFILTER_DISABLE;
	}
	else
	{
		FRAME_BUTTONS[28].button->setSelected(true);
		bilinearFilter = BILINEARFILTER_ENABLE;
	}
}

void Func_TrapFilter()
{
	if(trapFilter == TRAPFILTER_ENABLE)
	{
		FRAME_BUTTONS[29].button->setSelected(false);
		trapFilter = TRAPFILTER_DISABLE;
		VIDEO_SetTrapFilter(0);
	}
	else
	{
		FRAME_BUTTONS[29].button->setSelected(true);
		trapFilter = TRAPFILTER_ENABLE;
		VIDEO_SetTrapFilter(1);
	}
}

void Func_ForceNTSC()
{
	long oldPsxType = Config.PsxType;
	if(forceNTSC == FORCENTSC_ENABLE)
	{
		FRAME_BUTTONS[63].button->setSelected(false);
		forceNTSC = FORCENTSC_DISABLE;
		if (hasLoadedISO)
		{
			CheckPsxType();
		}
	}
	else
	{
		FRAME_BUTTONS[63].button->setSelected(true);
		forceNTSC = FORCENTSC_ENABLE;
		Config.PsxType = PSX_TYPE_NTSC;
	}

	if (hasLoadedISO && oldPsxType != Config.PsxType)
	{
		psxResetRcntRate();
		pl_chg_psxtype(Config.PsxType);
		gpuChangePsxType();
	}
}

void Func_ConfigureInput()
{
//	menu::MessageBox::getInstance().setMessage("Input configuration not implemented");
	pMenuContext->setActiveFrame(MenuContext::FRAME_CONFIGUREINPUT,ConfigureInputFrame::SUBMENU_REINIT);
}

void Func_ConfigureButtons()
{
//	menu::MessageBox::getInstance().setMessage("Button Mapping not implemented");
	pMenuContext->setActiveFrame(MenuContext::FRAME_CONFIGUREBUTTONS,ConfigureButtonsFrame::SUBMENU_PSX_PADNONE);
}

void Func_PsxTypeStandard()
{
	for (int i = 32; i <= 33; i++)
		FRAME_BUTTONS[i].button->setSelected(false);
	FRAME_BUTTONS[32].button->setSelected(true);
	controllerType = CONTROLLERTYPE_STANDARD;
}

void Func_PsxTypeAnalog()
{
	for (int i = 32; i <= 33; i++)
		FRAME_BUTTONS[i].button->setSelected(false);
	FRAME_BUTTONS[33].button->setSelected(true);
	controllerType = CONTROLLERTYPE_ANALOG;
}

void Func_PsxTypeLightgun()
{
    lightGun++;
	FRAME_BUTTONS[59].button->setSelected(true);
    if (lightGun > LIGHTGUN_MOUSE)
    {
        lightGun = LIGHTGUN_DISABLE;
		FRAME_BUTTONS[59].button->setSelected(false);
    }
    FRAME_BUTTONS[59].buttonString = FRAME_STRINGS[70 + lightGun];
}

void Func_DisableRumbleYes()
{
	for (int i = 34; i <= 35; i++)
		FRAME_BUTTONS[i].button->setSelected(false);
	FRAME_BUTTONS[34].button->setSelected(true);
	rumbleEnabled = RUMBLE_DISABLE;
}

void Func_DisableRumbleNo()
{
	for (int i = 34; i <= 35; i++)
		FRAME_BUTTONS[i].button->setSelected(false);
	FRAME_BUTTONS[35].button->setSelected(true);
	rumbleEnabled = RUMBLE_ENABLE;
}

void Func_Memcard1()
{
	if(memCard[0] == MEMCARD_ENABLE)
	{
		FRAME_BUTTONS[60].button->setSelected(false);
		memCard[0] = MEMCARD_DISABLE;
	}
	else
	{
		FRAME_BUTTONS[60].button->setSelected(true);
		memCard[0] = MEMCARD_ENABLE;
	}
}

void Func_Memcard2()
{
	if(memCard[1] == MEMCARD_ENABLE)
	{
		FRAME_BUTTONS[61].button->setSelected(false);
		memCard[1] = MEMCARD_DISABLE;
	}
	else
	{
		FRAME_BUTTONS[61].button->setSelected(true);
		memCard[1] = MEMCARD_ENABLE;
	}
}



void Func_SaveButtonsSD()
{
	fileBrowser_file* configFile_file;
	int (*configFile_init)(fileBrowser_file*) = fileBrowser_libfat_init;
	int num_written = 0;
	configFile_file = &saveDir_libfat_Default;
	if(configFile_init(configFile_file)) {                //only if device initialized ok
		FILE* f = fopen( "sd:/wiisxrx/controlG.cfg", "wb" );  //attempt to open file
		if(f) {
			save_configurations(f, &controller_GC);					//write out GC controller mappings
			fclose(f);
			num_written++;
		}
#ifdef HW_RVL
        f = fopen( "sd:/wiisxrx/controlH.cfg", "wb" );  //attempt to open file
		if(f) {
			save_configurations(f, &controller_HidGC);			//write out HID controller mappings
			fclose(f);
			num_written++;
		}
		f = fopen( "sd:/wiisxrx/controlC.cfg", "wb" );  //attempt to open file
		if(f) {
			save_configurations(f, &controller_Classic);			//write out Classic controller mappings
			fclose(f);
			num_written++;
		}
		f = fopen( "sd:/wiisxrx/controlN.cfg", "wb" );  //attempt to open file
		if(f) {
			save_configurations(f, &controller_WiimoteNunchuk);	//write out WM+NC controller mappings
			fclose(f);
			num_written++;
		}
		f = fopen( "sd:/wiisxrx/controlW.cfg", "wb" );  //attempt to open file
		if(f) {
			save_configurations(f, &controller_Wiimote);			//write out Wiimote controller mappings
			fclose(f);
			num_written++;
		}
		f = fopen("sd:/wiisxrx/controlP.cfg", "wb");  //attempt to open file
		if (f) {
			save_configurations(f, &controller_WiiUPro);			//write out Wii U Pro controller mappings
			fclose(f);
			num_written++;
		}
		f = fopen("sd:/wiisxrx/controlD.cfg", "wb");  //attempt to open file
		if (f) {
			save_configurations(f, &controller_WiiUGamepad);		//write out Wii U Gamepad controller mappings
			fclose(f);
			num_written++;
		}
#endif //HW_RVL
	}
	if (num_written == num_controller_t)
		menu::MessageBox::getInstance().setMessage("Saved Button Configs to SD");
	else
		menu::MessageBox::getInstance().setMessage("Error saving Button Configs to SD");
}

void Func_SaveButtonsUSB()
{
	fileBrowser_file* configFile_file;
	int (*configFile_init)(fileBrowser_file*) = fileBrowser_libfat_init;
	int num_written = 0;
	configFile_file = &saveDir_libfat_USB;
	if(configFile_init(configFile_file)) {                //only if device initialized ok
		FILE* f = fopen( "usb:/wiisxrx/controlG.cfg", "wb" );  //attempt to open file
		if(f) {
			save_configurations(f, &controller_GC);					//write out GC controller mappings
			fclose(f);
			num_written++;
		}
#ifdef HW_RVL
        f = fopen( "usb:/wiisxrx/controlH.cfg", "wb" );  //attempt to open file
		if(f) {
			save_configurations(f, &controller_HidGC);			//write out HID controller mappings
			fclose(f);
			num_written++;
		}
		f = fopen( "usb:/wiisxrx/controlC.cfg", "wb" );  //attempt to open file
		if(f) {
			save_configurations(f, &controller_Classic);			//write out Classic controller mappings
			fclose(f);
			num_written++;
		}
		f = fopen( "usb:/wiisxrx/controlN.cfg", "wb" );  //attempt to open file
		if(f) {
			save_configurations(f, &controller_WiimoteNunchuk);	//write out WM+NC controller mappings
			fclose(f);
			num_written++;
		}
		f = fopen( "usb:/wiisxrx/controlW.cfg", "wb" );  //attempt to open file
		if(f) {
			save_configurations(f, &controller_Wiimote);			//write out Wiimote controller mappings
			fclose(f);
			num_written++;
		}
		f = fopen("usb:/wiisxrx/controlP.cfg", "wb");  //attempt to open file
		if (f) {
			save_configurations(f, &controller_WiiUPro);			//write out Wii U Pro controller mappings
			fclose(f);
			num_written++;
		}
		f = fopen("usb:/wiisxrx/controlD.cfg", "wb");  //attempt to open file
		if (f) {
			save_configurations(f, &controller_WiiUGamepad);		//write out Wii U Gamepad controller mappings
			fclose(f);
			num_written++;
		}
#endif //HW_RVL
	}
	if (num_written == num_controller_t)
		menu::MessageBox::getInstance().setMessage("Saved Button Configs to USB");
	else
		menu::MessageBox::getInstance().setMessage("Error saving Button Configs to USB");
}

void Func_SetButtonLoad()
{
	if (loadButtonSlot == LOADBUTTON_DEFAULT)
		strcpy(FRAME_STRINGS[42], "Default");
	else
		sprintf(FRAME_STRINGS[42], "Slot %d", loadButtonSlot+1);
}

void Func_ToggleButtonLoad()
{
	loadButtonSlot = (loadButtonSlot + 1) % 5;
	Func_SetButtonLoad();
}

void Func_DisableAudioYes()
{
	for (int i = 39; i <= 40; i++)
		FRAME_BUTTONS[i].button->setSelected(false);
	FRAME_BUTTONS[39].button->setSelected(true);
	audioEnabled = AUDIO_DISABLE;
}

void Func_DisableAudioNo()
{
	for (int i = 39; i <= 40; i++)
		FRAME_BUTTONS[i].button->setSelected(false);
	FRAME_BUTTONS[40].button->setSelected(true);
	audioEnabled = AUDIO_ENABLE;
}

void Func_DisableXaYes()
{
	for (int i = 41; i <= 42; i++)
		FRAME_BUTTONS[i].button->setSelected(false);
	FRAME_BUTTONS[41].button->setSelected(true);
	Config.Xa = XA_DISABLE;
}

void Func_DisableXaNo()
{
	for (int i = 41; i <= 42; i++)
		FRAME_BUTTONS[i].button->setSelected(false);
	FRAME_BUTTONS[42].button->setSelected(true);
	Config.Xa = XA_ENABLE;
}

#ifdef SHOW_DEBUG
extern bool canWriteLog;
#endif // SHOW_DEBUG
void Func_DisableCddaYes()
{
	for (int i = 43; i <= 44; i++)
		FRAME_BUTTONS[i].button->setSelected(false);
	FRAME_BUTTONS[43].button->setSelected(true);
	Config.Cdda = CDDA_DISABLE;
	#ifdef SHOW_DEBUG
	canWriteLog = !canWriteLog;
	sprintf(txtbuffer,"Current Write Log Status %d", canWriteLog);
	menu::MessageBox::getInstance().setMessage(txtbuffer);
	#endif // SHOW_DEBUG
}

void Func_DisableCddaNo()
{
	for (int i = 43; i <= 44; i++)
		FRAME_BUTTONS[i].button->setSelected(false);
	FRAME_BUTTONS[44].button->setSelected(true);
	Config.Cdda = CDDA_ENABLE;
	#ifdef SHOW_DEBUG
	canWriteLog = !canWriteLog;
	sprintf(txtbuffer,"Current Write Log Status %d", canWriteLog);
	menu::MessageBox::getInstance().setMessage(txtbuffer);
	#endif // SHOW_DEBUG
	//menu::MessageBox::getInstance().setMessage("CDDA audio is not implemented");
}

void Func_VolumeToggle()
{
	if (spuInterpolation == SIMPLE_INTERPOLATION)
	{
		spuInterpolation = GAUSSI_INTERPOLATION;
	}
	else
	{
		spuInterpolation = SIMPLE_INTERPOLATION;
	}
	FRAME_BUTTONS[45].buttonString = FRAME_STRINGS[46 + spuInterpolation];
	setSpuInterpolation(spuInterpolation);
}

void Func_MemcardSaveSD()
{
	for (int i = 46; i <= 49; i++)
		FRAME_BUTTONS[i].button->setSelected(false);
	FRAME_BUTTONS[46].button->setSelected(true);
	nativeSaveDevice = NATIVESAVEDEVICE_SD;
}

void Func_MemcardSaveUSB()
{
	for (int i = 46; i <= 49; i++)
		FRAME_BUTTONS[i].button->setSelected(false);
	FRAME_BUTTONS[47].button->setSelected(true);
	nativeSaveDevice = NATIVESAVEDEVICE_USB;
}

void Func_MemcardSaveCardA()
{
	for (int i = 46; i <= 49; i++)
		FRAME_BUTTONS[i].button->setSelected(false);
	FRAME_BUTTONS[48].button->setSelected(true);
	nativeSaveDevice = NATIVESAVEDEVICE_CARDA;
}

void Func_MemcardSaveCardB()
{
	for (int i = 46; i <= 49; i++)
		FRAME_BUTTONS[i].button->setSelected(false);
	FRAME_BUTTONS[49].button->setSelected(true);
	nativeSaveDevice = NATIVESAVEDEVICE_CARDB;
}

void Func_AutoSaveYes()
{
	for (int i = 50; i <= 51; i++)
		FRAME_BUTTONS[i].button->setSelected(false);
	FRAME_BUTTONS[50].button->setSelected(true);
	autoSave = AUTOSAVE_ENABLE;
}

void Func_AutoSaveNo()
{
	for (int i = 50; i <= 51; i++)
		FRAME_BUTTONS[i].button->setSelected(false);
	FRAME_BUTTONS[51].button->setSelected(true);
	autoSave = AUTOSAVE_DISABLE;
}

void Func_SaveStateSD()
{
	for (int i = 52; i <= 53; i++)
		FRAME_BUTTONS[i].button->setSelected(false);
	FRAME_BUTTONS[52].button->setSelected(true);
	saveStateDevice = SAVESTATEDEVICE_SD;
}

void Func_SaveStateUSB()
{
	for (int i = 52; i <= 53; i++)
		FRAME_BUTTONS[i].button->setSelected(false);
	FRAME_BUTTONS[53].button->setSelected(true);
	saveStateDevice = SAVESTATEDEVICE_USB;
}

void Func_FastloadYes()
{
	fastLoad = 1;
	FRAME_BUTTONS[55].button->setSelected(true);
	FRAME_BUTTONS[56].button->setSelected(false);
}

void Func_FastloadNo()
{
    fastLoad = 0;
	FRAME_BUTTONS[55].button->setSelected(false);
	FRAME_BUTTONS[56].button->setSelected(true);
}

void Func_ReturnFromSettingsFrame()
{
	menu::Gui::getInstance().menuLogo->setLocation(580.0, 70.0, -50.0);
	pMenuContext->setActiveFrame(MenuContext::FRAME_MAIN);
}

