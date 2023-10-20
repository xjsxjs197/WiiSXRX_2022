/**
 * WiiSX - wiiSXconfig.h
 * Copyright (C) 2007, 2008, 2009, 2010 sepp256
 *
 * External declaration and enumeration of config variables
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


#ifndef WIISXCONFIG_H
#define WIISXCONFIG_H


extern char audioEnabled;
enum audioEnabled
{
	AUDIO_DISABLE=0,
	AUDIO_ENABLE
};

enum ConfigXa //Config.Xa
{
	XA_ENABLE=0,
	XA_DISABLE
};

enum ConfigCdda //Config.Cdda
{
	CDDA_ENABLE=0,
	CDDA_DISABLE
};

extern int iVolume;
extern char volume;
enum iVolume
{
	VOLUME_LOUDEST=1,
	VOLUME_LOUD,
	VOLUME_MEDIUM,
	VOLUME_LOW
};

extern char showFPSonScreen;
enum showFPSonScreen
{
	FPS_HIDE=0,
	FPS_SHOW
};

extern char printToScreen;
enum printToScreen
{
	DEBUG_HIDE=0,
	DEBUG_SHOW
};

extern char printToSD;
enum printToSD
{
	SDLOG_DISABLE=0,
	SDLOG_ENABLE
};

extern char frameLimit[2];
enum frameLimit
{
	FRAMELIMIT_NONE=0,
	FRAMELIMIT_AUTO,
};

extern char frameSkip;
enum frameSkip
{
	FRAMESKIP_DISABLE=0,
	FRAMESKIP_ENABLE,
};

extern int iUseDither;
extern char useDithering;
enum iUseDither
{
	USEDITHER_NONE=0,
	USEDITHER_DEFAULT,
	USEDITHER_ALWAYS
};

extern char saveEnabled;	//???

extern char nativeSaveDevice;
enum nativeSaveDevice
{
	NATIVESAVEDEVICE_NONE=-1,
	NATIVESAVEDEVICE_SD,
	NATIVESAVEDEVICE_USB,
	NATIVESAVEDEVICE_CARDA,
	NATIVESAVEDEVICE_CARDB
};

extern char saveStateDevice;
enum saveStateDevice
{
	SAVESTATEDEVICE_SD=0,
	SAVESTATEDEVICE_USB
};

extern char autoSave;
enum autoSave
{
	AUTOSAVE_DISABLE=0,
	AUTOSAVE_ENABLE
};

extern char creditsScrolling;	//deprecated?

extern char dynacore;
enum dynacore
{

	DYNACORE_DYNAREC=0, // Lightrec
	DYNACORE_INTERPRETER, // Interpreter
	DYNACORE_DYNAREC_OLD // old PPC dynarec
};

extern char biosDevice;
enum biosDevice
{
	BIOSDEVICE_HLE=0,
	BIOSDEVICE_SD,
	BIOSDEVICE_USB
};

extern char LoadCdBios;
enum loadCdBios
{
	BOOTTHRUBIOS_NO=0,
	BOOTTHRUBIOS_YES
};

extern char screenMode;
enum screenMode
{
	SCREENMODE_4x3=0,
	SCREENMODE_16x9,
	SCREENMODE_16x9_PILLARBOX
};

extern char videoMode;
enum videoMode
{
	VIDEOMODE_AUTO=0,
	VIDEOMODE_NTSC,
	VIDEOMODE_PAL50,
	VIDEOMODE_PAL60,
	VIDEOMODE_PROGRESSIVE
};

extern char fileSortMode;
enum fileSortMode
{
	FILESORT_DIRS_MIXED=0,
	FILESORT_DIRS_FIRST
};

extern char padAutoAssign;
enum padAutoAssign
{
	PADAUTOASSIGN_MANUAL=0,
	PADAUTOASSIGN_AUTOMATIC
};

extern char padType[10];
enum padType
{
	PADTYPE_NONE=0,
	PADTYPE_GAMECUBE,
	PADTYPE_WII,
	PADTYPE_MULTITAP
	
};

extern char padAssign[10];
enum padAssign
{
	PADASSIGN_INPUT0=0,
	PADASSIGN_INPUT1,
	PADASSIGN_INPUT0A,
	PADASSIGN_INPUT0B,
	PADASSIGN_INPUT0C,
	PADASSIGN_INPUT0D,
	PADASSIGN_INPUT1A,
	PADASSIGN_INPUT1B,
	PADASSIGN_INPUT1C,
	PADASSIGN_INPUT1D,
};

extern char padLightgun[10];
enum padLightgun
{
	PADLIGHTGUN_DISABLE=0,
	PADLIGHTGUN_ENABLE,
	
};

extern char rumbleEnabled;
enum rumbleEnabled
{
	RUMBLE_DISABLE=0,
	RUMBLE_ENABLE
};

extern char loadButtonSlot;
enum loadButtonSlot
{
	LOADBUTTON_SLOT0=0,
	LOADBUTTON_SLOT1,
	LOADBUTTON_SLOT2,
	LOADBUTTON_SLOT3,
	LOADBUTTON_DEFAULT
};

extern char controllerType;
enum controllerType
{
	CONTROLLERTYPE_STANDARD=0,
	CONTROLLERTYPE_ANALOG,
	CONTROLLERTYPE_LIGHTGUN
};

extern char numMultitaps;
enum numMultitaps
{
	MULTITAPS_NONE=0,
	MULTITAPS_ONE,
	MULTITAPS_TWO
};

extern char lang;
extern char oldLang;
enum lang
{
	ENGLISH = 0,
	SIMP_CHINESE,
	KOREAN,
	SPANISH,
	PORTUGUESE,
	ITALIAN,
	TRAD_CHINESE,
	JAPANESE,
	GERMAN,
	FRENCH
};

extern char originalMode;
enum originalMode
{
	ORIGINALMODE_DISABLE=0,
	ORIGINALMODE_ENABLE,
};

extern char bilinearFilter;
enum bilinearFilter
{
	BILINEARFILTER_DISABLE=0,
	BILINEARFILTER_ENABLE,
};

extern char trapFilter;
enum trapFilter
{
	TRAPFILTER_DISABLE=0,
	TRAPFILTER_ENABLE,
};

extern char interlacedMode;
enum interlacedMode
{
	INTERLACED_DISABLE=0,
	INTERLACED_ENABLE,
};

extern char deflickerFilter;
enum deflickerFilter
{
	DEFLICKER_DISABLE=0,
	DEFLICKER_ENABLE,
};

extern char lightGun;
enum lightGun
{
	LIGHTGUN_DISABLE=0,
	LIGHTGUN_GUNCON,
	LIGHTGUN_JUST,
	LIGHTGUN_MOUSE,
};

extern char memCard[2];
enum memCard
{
	MEMCARD_DISABLE=0,
	MEMCARD_ENABLE,
};


extern const unsigned char En_dat[];
extern const unsigned int  En_dat_size;
extern char canChangeFont;

#endif //WIISXCONFIG_H
