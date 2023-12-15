/*  Pcsx - Pc Psx Emulator
 *  Copyright (C) 1999-2002  Pcsx Team
 *  Copyright (C) 2009-2010  WiiSX Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <gccore.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <stdarg.h>
#include <errno.h>

#include <time.h>
#include <fat.h>
#include <aesndlib.h>
#include <sys/iosupport.h>

#ifdef DEBUGON
# include <debug.h>
#endif

#include "../psxcommon.h"
#include "wiiSXconfig.h"
#include "menu/MenuContext.h"
#include "libgui/IPLFont.h"
#include "libgui/MessageBox.h"

extern char * GetGameBios(char * biosPath, char * fileName, int isoFileNameLen);
extern char* filenameFromAbsPath(char* absPath);
extern u32 __di_check_ahbprot(void);
extern unsigned int cdrIsoMultidiskSelect;
extern bool executedBios;

extern "C" {
#include "DEBUG.h"
#include "fileBrowser/fileBrowser.h"
#include "fileBrowser/fileBrowser-libfat.h"
#include "fileBrowser/fileBrowser-DVD.h"
#include "fileBrowser/fileBrowser-CARD.h"
#include "fileBrowser/fileBrowser-SMB.h"
#include "gc_input/controller.h"
#include "vm/vm.h"
}

#include "libgui/gui2/gettext.h"

#ifdef WII
unsigned int MALLOC_MEM2 = 0;
extern "C" {
#include <di/di.h>
}
#endif //WII

/* function prototypes */
extern "C" {
int SysInit();
void SysReset();
void SysClose();
void *SysLoadLibrary(char *lib);
void *SysLoadSym(void *lib, char *sym);
void SysCloseLibrary(void *lib);
void SysUpdate();
void SysRunGui();
void SysMessage(char *fmt, ...);
void LidInterrupt();
}

u32* xfb[3] = { NULL, NULL, NULL };	/*** Framebuffers ***/
GXRModeObj *vmode;				/*** Graphics Mode Object ***/
BOOL hasLoadedISO = FALSE;
fileBrowser_file isoFile;  //the ISO file
fileBrowser_file cddaFile; //the CDDA file
fileBrowser_file subFile;  //the SUB file
fileBrowser_file *biosFile = NULL;  //BIOS file

#if defined (CPU_LOG) || defined(DMA_LOG) || defined(CDR_LOG) || defined(HW_LOG) || \
	defined(PSXBIOS_LOG) || defined(GTE_LOG) || defined(PAD_LOG)
FILE *emuLog;
#endif

PcsxConfig Config;
char dynacore;
char biosDevice;
char LoadCdBios=0;
char frameLimit[2];
char frameSkip;
char useDithering;
extern char audioEnabled;
char volume;
char showFPSonScreen;
char printToScreen;
char menuActive;
char saveEnabled;
char creditsScrolling;
char padNeedScan=1;
char wpadNeedScan=1;
char shutdown = 0;
char nativeSaveDevice;
char saveStateDevice;
char autoSave;
signed char autoSaveLoaded = 0;
char screenMode = 0;
char videoMode = 0;
char fileSortMode = 1;
char padAutoAssign;
char padType[10];
char padAssign[10];
char padLightgun[10];
char rumbleEnabled;
char loadButtonSlot;
char controllerType;
char numMultitaps;
char lang = 0;
char fastLoad = 0;
char originalMode = 0;
char bilinearFilter = 1;
char trapFilter = 1;
char interlacedMode = 0;
char deflickerFilter = 1;
char lightGun = 0;
char memCard[2];

#define CONFIG_STRING_TYPE 0
#define CONFIG_STRING_SIZE 256
char smbUserName[CONFIG_STRING_SIZE];
char smbPassWord[CONFIG_STRING_SIZE];
char smbShareName[CONFIG_STRING_SIZE];
char smbIpAddr[CONFIG_STRING_SIZE];

int stop = 0;
bool needInitCpu = true;

static struct {
	const char* key;
	// For integral options this is a pointer to a char
	// for string values, this is a pointer to a 256-byte string
	// thus, assigning a string value to an integral type will cause overflow
	char* value;
	char  min, max;
} OPTIONS[] =
{ { "Audio", &audioEnabled, AUDIO_DISABLE, AUDIO_ENABLE },
  { "Volume", &volume, VOLUME_LOUDEST, VOLUME_LOW },
  { "FPS", &showFPSonScreen, FPS_HIDE, FPS_SHOW },
//  { "Debug", &printToScreen, DEBUG_HIDE, DEBUG_SHOW },
  { "ScreenMode", &screenMode, SCREENMODE_4x3, SCREENMODE_16x9_PILLARBOX },
  { "VideoMode", &videoMode, VIDEOMODE_AUTO, VIDEOMODE_PROGRESSIVE },
  { "FileSortMode", &fileSortMode, FILESORT_DIRS_MIXED, FILESORT_DIRS_FIRST },
  { "Core", &dynacore, DYNACORE_DYNAREC, DYNACORE_DYNAREC_OLD },
  { "NativeDevice", &nativeSaveDevice, NATIVESAVEDEVICE_SD, NATIVESAVEDEVICE_CARDB },
  { "StatesDevice", &saveStateDevice, SAVESTATEDEVICE_SD, SAVESTATEDEVICE_USB },
  { "AutoSave", &autoSave, AUTOSAVE_DISABLE, AUTOSAVE_ENABLE },
  { "BiosDevice", &biosDevice, BIOSDEVICE_HLE, BIOSDEVICE_USB },
  { "BootThruBios", &LoadCdBios, BOOTTHRUBIOS_NO, BOOTTHRUBIOS_YES },
  { "LimitFrames", &frameLimit[1], FRAMELIMIT_NONE, FRAMELIMIT_AUTO },
  { "SkipFrames", &frameSkip, FRAMESKIP_DISABLE, FRAMESKIP_ENABLE },
  { "Dithering", &useDithering, USEDITHER_NONE, USEDITHER_ALWAYS },
  { "PadAutoAssign", &padAutoAssign, PADAUTOASSIGN_MANUAL, PADAUTOASSIGN_AUTOMATIC },
  { "PadType1", &padType[0], PADTYPE_NONE, PADTYPE_MULTITAP },
  { "PadType2", &padType[1], PADTYPE_NONE, PADTYPE_MULTITAP },
  { "PadType3", &padType[2], PADTYPE_NONE, PADTYPE_MULTITAP },
  { "PadType4", &padType[3], PADTYPE_NONE, PADTYPE_MULTITAP },
  { "PadType5", &padType[4], PADTYPE_NONE, PADTYPE_MULTITAP },
  { "PadType6", &padType[5], PADTYPE_NONE, PADTYPE_MULTITAP },
  { "PadType7", &padType[6], PADTYPE_NONE, PADTYPE_MULTITAP },
  { "PadType8", &padType[7], PADTYPE_NONE, PADTYPE_MULTITAP },
  { "PadType9", &padType[8], PADTYPE_NONE, PADTYPE_MULTITAP },
  { "PadType10", &padType[9], PADTYPE_NONE, PADTYPE_MULTITAP },
  { "PadAssign1", &padAssign[0], PADASSIGN_INPUT0, PADASSIGN_INPUT1D },
  { "PadAssign2", &padAssign[1], PADASSIGN_INPUT0, PADASSIGN_INPUT1D },
  { "PadAssign3", &padAssign[2], PADASSIGN_INPUT0, PADASSIGN_INPUT1D },
  { "PadAssign4", &padAssign[3], PADASSIGN_INPUT0, PADASSIGN_INPUT1D },
  { "PadAssign5", &padAssign[4], PADASSIGN_INPUT0, PADASSIGN_INPUT1D },
  { "PadAssign6", &padAssign[5], PADASSIGN_INPUT0, PADASSIGN_INPUT1D },
  { "PadAssign7", &padAssign[6], PADASSIGN_INPUT0, PADASSIGN_INPUT1D },
  { "PadAssign8", &padAssign[7], PADASSIGN_INPUT0, PADASSIGN_INPUT1D },
  { "PadAssign9", &padAssign[8], PADASSIGN_INPUT0, PADASSIGN_INPUT1D },
  { "PadAssign10", &padAssign[9], PADASSIGN_INPUT0, PADASSIGN_INPUT1D },
  { "RumbleEnabled", &rumbleEnabled, RUMBLE_DISABLE, RUMBLE_ENABLE },
  { "LoadButtonSlot", &loadButtonSlot, LOADBUTTON_SLOT0, LOADBUTTON_DEFAULT },
  { "ControllerType", &controllerType, CONTROLLERTYPE_STANDARD, CONTROLLERTYPE_ANALOG },
//  { "NumberMultitaps", &numMultitaps, MULTITAPS_NONE, MULTITAPS_TWO },
  { "smbusername", smbUserName, CONFIG_STRING_TYPE, CONFIG_STRING_TYPE },
  { "smbpassword", smbPassWord, CONFIG_STRING_TYPE, CONFIG_STRING_TYPE },
  { "smbsharename", smbShareName, CONFIG_STRING_TYPE, CONFIG_STRING_TYPE },
  { "smbipaddr", smbIpAddr, CONFIG_STRING_TYPE, CONFIG_STRING_TYPE },
  { "lang", &lang, ENGLISH, TURKISH },
  { "fastLoad", &fastLoad, 0, 1 },
  { "TVMode", &originalMode, ORIGINALMODE_DISABLE, ORIGINALMODE_ENABLE },
  { "BilinearFilter", &bilinearFilter, BILINEARFILTER_DISABLE, BILINEARFILTER_ENABLE },
  { "TrapFilter", &trapFilter, TRAPFILTER_DISABLE, TRAPFILTER_ENABLE },
  { "Interlaced", &interlacedMode, INTERLACED_DISABLE, INTERLACED_ENABLE },
  { "DeflickerFilter", &deflickerFilter, DEFLICKER_DISABLE, DEFLICKER_ENABLE },
  { "LightGun", &lightGun, LIGHTGUN_DISABLE, LIGHTGUN_MOUSE },
  { "Memcard0", &memCard[0], MEMCARD_DISABLE, MEMCARD_ENABLE },
  { "Memcard1", &memCard[1], MEMCARD_DISABLE, MEMCARD_ENABLE },
  { "PadLightgun1", &padLightgun[0], PADLIGHTGUN_DISABLE, PADLIGHTGUN_ENABLE },
  { "PadLightgun2", &padLightgun[1], PADLIGHTGUN_DISABLE, PADLIGHTGUN_ENABLE },
  { "PadLightgun3", &padLightgun[2], PADLIGHTGUN_DISABLE, PADLIGHTGUN_ENABLE },
  { "PadLightgun4", &padLightgun[3], PADLIGHTGUN_DISABLE, PADLIGHTGUN_ENABLE },
  { "PadLightgun5", &padLightgun[4], PADLIGHTGUN_DISABLE, PADLIGHTGUN_ENABLE },
  { "PadLightgun6", &padLightgun[5], PADLIGHTGUN_DISABLE, PADLIGHTGUN_ENABLE },
  { "PadLightgun7", &padLightgun[6], PADLIGHTGUN_DISABLE, PADLIGHTGUN_ENABLE },
  { "PadLightgun8", &padLightgun[7], PADLIGHTGUN_DISABLE, PADLIGHTGUN_ENABLE },
  { "PadLightgun9", &padLightgun[8], PADLIGHTGUN_DISABLE, PADLIGHTGUN_ENABLE },
  { "PadLightgun10", &padLightgun[9], PADLIGHTGUN_DISABLE, PADLIGHTGUN_ENABLE }
};
void handleConfigPair(char* kv);
void readConfig(FILE* f);
void writeConfig(FILE* f);
int checkBiosExists(int testDevice);
static void loadSeparatelySetting();
static bool loadSeparatelySettingItem(char* s1, char* s2, bool isUsb);
static void biosFileInit();

static bool loadControllerMapping(char* usbSd)
{
    char settingPathBuf[256];
    FILE* f;
    bool loadRet = true;

    sprintf(settingPathBuf, "%s:/wiisxrx/controlG.cfg", usbSd);
    f = fopen(settingPathBuf, "rb" );  //attempt to open file
    if (f) {
        load_configurations(f, &controller_GC);					//read in GC controller mappings
        fclose(f);
    }
    else
    {
        loadRet = false;
    }

    #ifdef HW_RVL

    sprintf(settingPathBuf, "%s:/wiisxrx/controlC.cfg", usbSd);
    f = fopen(settingPathBuf, "rb" );  //attempt to open file
    if(f) {
        load_configurations(f, &controller_Classic);			//read in Classic controller mappings
        fclose(f);
    }
    else
    {
        loadRet = false;
    }

    sprintf(settingPathBuf, "%s:/wiisxrx/controlN.cfg", usbSd);
    f = fopen(settingPathBuf, "rb" );  //attempt to open file
    if(f) {
        load_configurations(f, &controller_WiimoteNunchuk);		//read in WM+NC controller mappings
        fclose(f);
    }
    else
    {
        loadRet = false;
    }

    sprintf(settingPathBuf, "%s:/wiisxrx/controlW.cfg", usbSd);
    f = fopen(settingPathBuf, "rb" );  //attempt to open file
    if(f) {
        load_configurations(f, &controller_Wiimote);			//read in Wiimote controller mappings
        fclose(f);
    }
    else
    {
        loadRet = false;
    }

    sprintf(settingPathBuf, "%s:/wiisxrx/controlP.cfg", usbSd);
    f = fopen(settingPathBuf, "rb" );  //attempt to open file
    if (f) {
        load_configurations(f, &controller_WiiUPro);			//read in Wii U Pro controller mappings
        fclose(f);
    }
    else
    {
        loadRet = false;
    }

    sprintf(settingPathBuf, "%s:/wiisxrx/controlD.cfg", usbSd);
    f = fopen(settingPathBuf, "rb" );  //attempt to open file
    if (f) {
        load_configurations(f, &controller_WiiUGamepad);		//read in Wii U Gamepad controller mappings
        fclose(f);
    }
    else
    {
        loadRet = false;
    }

    #endif // HW_RVL

    return loadRet;
}

void loadSettings(int argc, char *argv[])
{
	// Default Settings
	audioEnabled     = 1; // Audio
	volume           = VOLUME_MEDIUM;
#ifdef RELEASE
	showFPSonScreen  = 0; // Don't show FPS on Screen
#else
	showFPSonScreen  = 1; // Show FPS on Screen
#endif
	printToScreen    = 1; // Show DEBUG text on screen
	printToSD        = 0; // Disable SD logging
	frameLimit[1]		 = 1; // Auto limit FPS
	frameSkip		 = 0; // Disable frame skipping
	useDithering		 = 1; // Default dithering (set to 0 (disabled) in PEOPSgpu)
	saveEnabled      = 0; // Don't save game
	nativeSaveDevice = 0; // SD
	saveStateDevice	 = 0; // SD
	autoSave         = 1; // Auto Save Game
	creditsScrolling = 0; // Normal menu for now
	dynacore         = 0; // Dynarec
	screenMode		 = 0; // Stretch FB horizontally
	videoMode		 = VIDEOMODE_AUTO;
	fileSortMode	 = FILESORT_DIRS_FIRST;
	padAutoAssign	 = PADAUTOASSIGN_AUTOMATIC;
	for (int i = 0; i < 10; i++){
		padType[i]		 = PADTYPE_NONE;
		padAssign[i]	 = PADASSIGN_INPUT0;
		padLightgun[i]	 = PADLIGHTGUN_ENABLE;
	}
	padAssign[1]	 = PADASSIGN_INPUT1;
	memCard[0]		 = MEMCARD_ENABLE;
	memCard[1]		 = MEMCARD_ENABLE;
	rumbleEnabled	 = RUMBLE_ENABLE;
	loadButtonSlot	 = LOADBUTTON_DEFAULT;
	controllerType	 = CONTROLLERTYPE_STANDARD;
	numMultitaps	 = MULTITAPS_NONE;
	menuActive = 1;

	//PCSX-specific defaults
	memset(&Config, 0, sizeof(PcsxConfig));
	Config.Cpu=dynacore;		//Dynarec core
	strcpy(Config.Net,"Disabled");
	Config.PsxOut = 1;
	Config.HLE = 1;
	Config.Xa = 0;  //XA enabled
	Config.Cdda = 0; //CDDA enabled
	Config.cycle_multiplier = CYCLE_MULT_DEFAULT;
	iVolume = volume; //Volume="medium" in PEOPSspu
	Config.PsxAuto = 1; //Autodetect
	LoadCdBios = BOOTTHRUBIOS_NO;
	lang = 0;
	fastLoad = 0;
	originalMode = 0;

	//config stuff
	int (*configFile_init)(fileBrowser_file*) = fileBrowser_libfat_init;
#ifdef HW_RVL
	if(argc && argv[0][0] == 'u') {  //assume USB
		fileBrowser_file* configFile_file = &saveDir_libfat_USB;
		if(configFile_init(configFile_file)) {                //only if device initialized ok
            memset(Config.PatchesDir, '\0', sizeof(Config.PatchesDir));
            strcpy(Config.PatchesDir, "usb:/wiisxrx/ppf/");
			FILE* f = fopen( "usb:/wiisxrx/settingsRX2022.cfg", "r" );  //attempt to open file
			if(f) {        //open ok, read it
				readConfig(f);
				fclose(f);
			}
			loadControllerMapping("usb");
		}
	}
	else /*if((argv[0][0]=='s') || (argv[0][0]=='/'))*/
#endif //HW_RVL
	{ //assume SD
		fileBrowser_file* configFile_file = &saveDir_libfat_Default;
		if(configFile_init(configFile_file)) {                //only if device initialized ok
            // add xjsxjs197 start
            memset(Config.PatchesDir, '\0', sizeof(Config.PatchesDir));
            strcpy(Config.PatchesDir, "sd:/wiisxrx/ppf/");
            // add xjsxjs197 end
			FILE* f = fopen( "sd:/wiisxrx/settingsRX2022.cfg", "r" );  //attempt to open file
			if(f) {        //open ok, read it
				readConfig(f);
				fclose(f);
			}

			loadControllerMapping("sd");
		}
	}
#ifdef HW_RVL
	// Handle options passed in through arguments
	int i;
	for(i=1; i<argc; ++i){
		handleConfigPair(argv[i]);
	}
#endif

	//Test for Bios file
	if(biosDevice != BIOSDEVICE_HLE)
		if(checkBiosExists((int)biosDevice) == FILE_BROWSER_ERROR_NO_FILE)
			biosDevice = BIOSDEVICE_HLE;

	//Synch settings with Config
	Config.Cpu=dynacore;
	//iUseDither = useDithering;
	iVolume = volume;
}

void ScanPADSandReset(u32 dummy)
{
//	PAD_ScanPads();
	padNeedScan = wpadNeedScan = 1;
	if(!((*(u32*)0xCC003000)>>16))
		stop = 1;
}

#ifdef HW_RVL
void ShutdownWii()
{
	shutdown = 1;
	stop = 1;
}
#endif

void video_mode_init(GXRModeObj *videomode, u32 *fb1, u32 *fb2, u32 *fb3)
{
	vmode = videomode;
	xfb[0] = fb1;
	xfb[1] = fb2;
	xfb[2] = fb3;
}

// Plugin structure
extern "C" {
#include "GamecubePlugins.h"
PluginTable plugins[] =
	{ PLUGIN_SLOT_0,
	  PLUGIN_SLOT_1,
	  PLUGIN_SLOT_2,
	  PLUGIN_SLOT_3,
	  PLUGIN_SLOT_4,
	  PLUGIN_SLOT_5,
	  PLUGIN_SLOT_6,
	  PLUGIN_SLOT_7 };
}

/****************************************************************************
 * IOS Check
 ***************************************************************************/
#ifdef HW_RVL
bool SupportedIOS(u32 ios)
{
        if(ios == 58 || ios == 61)
                return true;

        return false;
}

bool SaneIOS(u32 ios)
{
        bool res = false;
        u32 num_titles=0;
        u32 tmd_size;

        if(ios > 200)
                return false;

        if (ES_GetNumTitles(&num_titles) < 0)
                return false;

        if(num_titles < 1)
                return false;

        u64 *titles = (u64 *)memalign(32, num_titles * sizeof(u64) + 32);

        if(!titles)
                return false;

        if (ES_GetTitles(titles, num_titles) < 0)
        {
                free(titles);
                return false;
        }

        u32 *tmdbuffer = (u32 *)memalign(32, MAX_SIGNED_TMD_SIZE);

        if(!tmdbuffer)
        {
                free(titles);
                return false;
        }

        for(u32 n=0; n < num_titles; n++)
        {
                if((titles[n] & 0xFFFFFFFF) != ios)
                        continue;

                if (ES_GetStoredTMDSize(titles[n], &tmd_size) < 0)
                        break;

                if (tmd_size > 4096)
                        break;

                if (ES_GetStoredTMD(titles[n], (signed_blob *)tmdbuffer, tmd_size) < 0)
                        break;

                if (tmdbuffer[1] || tmdbuffer[2])
                {
                        res = true;
                        break;
                }
        }
        free(tmdbuffer);
    free(titles);
        return res;
}
#endif

bool Autoboot;
char AutobootROM[1024];
char AutobootPath[1024];

int main(int argc, char *argv[])
{
	/* INITIALIZE */
#ifdef HW_RVL
	if(argc > 2 && argv[1] != NULL && argv[2] != NULL)
	{
		Autoboot = true;
		strncpy(AutobootPath, argv[1], sizeof(AutobootPath));
		strncpy(AutobootROM, argv[2], sizeof(AutobootROM));
	}
	else
	{
		Autoboot = false;
		memset(AutobootPath, 0, sizeof(AutobootPath));
		memset(AutobootROM, 0, sizeof(AutobootROM));
	}

		L2Enhance();

        u32 ios = IOS_GetVersion();

        if(!SupportedIOS(ios))
        {
                s32 preferred = IOS_GetPreferredVersion();

                if(SupportedIOS(preferred))
                        IOS_ReloadIOS(preferred);
        }

	#endif

	control_info_init(); //Perform controller auto assignment at least once at startup.

	loadSettings(argc, argv);

	#ifdef HW_RVL
	VM_Init(1024*1024, 256*1024); // whatever for now, we're not really using this for anything other than mmap on Wii.
	#endif // HW_RVL

	LoadLanguage();
	ChangeLanguage();

	MenuContext *menu = new MenuContext(vmode);
	VIDEO_SetPostRetraceCallback (ScanPADSandReset);

#ifndef WII
	DVD_Init();
#endif

#ifdef DEBUGON
	//DEBUG_Init(GDBSTUB_DEVICE_TCP,GDBSTUB_DEF_TCPPORT); //Default port is 2828
	DEBUG_Init(GDBSTUB_DEVICE_USB, 1);
	_break();
#endif

	// Start up AESND (inited here because its used in SPU and CD)
	//AESND_Init();

#ifdef HW_RVL
	// Initialize the network if the user has specified something in their SMB settings
	if(strlen(&smbShareName[0]) && strlen(&smbIpAddr[0])) {
	  init_network_thread();
  }
#endif

	if(Autoboot)
	{
		menu->Autoboot();
		Autoboot = false;
	}

	while (menu->isRunning()) {}

	// Shut down AESND
	//AESND_Reset();

    menu::IplFont obj = menu::IplFont::getInstance();
	delete &obj;

	delete menu;

	ReleaseLanguage();

	return 0;
}

void psxCpuInit()
{
    if (Config.Cpu == DYNACORE_INTERPRETER) {
		psxCpu = &psxInt;
	}
#if defined(__x86_64__) || defined(__i386__) || defined(__sh__) || defined(__ppc__) || defined(HW_RVL) || defined(HW_DOL)
	if (Config.Cpu == DYNACORE_DYNAREC)
	{
		psxCpu = &psxLightrec;
	}
	if (Config.Cpu == DYNACORE_DYNAREC_OLD)
	{
		psxCpu = &psxRec;
	}
#endif

    psxCpu->Init();
}

static void biosFileInit()
{
	biosFile_dir = &biosDir_libfat_Default;
	if (biosDevice != BIOSDEVICE_HLE) {
		Config.HLE = BIOS_USER_DEFINED;
		biosFile_dir = (biosDevice == BIOSDEVICE_SD) ? &biosDir_libfat_Default : &biosDir_libfat_USB;
	} else {
		Config.HLE = BIOS_HLE;
	}

	// Even in HLE mode, the corresponding BIOS file needs to be loaded for games
	// that have modified the BIOS font library
	biosFile_readFile  = fileBrowser_libfat_readFile;
	biosFile_open      = fileBrowser_libfat_open;
	biosFile_init      = fileBrowser_libfat_init;
	biosFile_deinit    = fileBrowser_libfat_deinit;
	if (biosFile) {
		free(biosFile);
 	}
	biosFile = (fileBrowser_file*)memalign(32,sizeof(fileBrowser_file));
	memcpy(biosFile,biosFile_dir,sizeof(fileBrowser_file));
	strcat(biosFile->name, GetGameBios(biosFile->name, filenameFromAbsPath(isoFile.name), strlen(isoFile.name)));
	biosFile_init(biosFile);  //initialize the bios device (it might not be the same as ISO device)
}

static bool loadSeparatelySettingItem(char* s1, char* s2, bool isUsb)
{
    struct stat s;
    char settingPathBuf[256];
    fileBrowser_file* configFile_file;
    sprintf(settingPathBuf, "%s%s%s", s1, s2, ".cfg");
    if (stat(settingPathBuf, &s))
    {
        return false;
    }

    configFile_file = isUsb ? &saveDir_libfat_USB : &saveDir_libfat_Default;
    int (*configFile_init)(fileBrowser_file*) = fileBrowser_libfat_init;
    if (configFile_init(configFile_file)) {        //only if device initialized ok
        FILE* f = fopen( settingPathBuf, "r" );   //attempt to open file
        if (f) {
            readConfig(f);
            fclose(f);
            return true;
        }
    }
    return false;
}

static void loadSeparatelySetting()
{
    char oldLoadButtonSlot = loadButtonSlot;

    // First, we load separately game settings.
    // Load separately game settings from USB device
    if (!loadSeparatelySettingItem("usb:/wiisxrx/settings/", CdromId, true))
    {
        // If there is no separate setting for USB
        // Load separately game settings from SD card
        if (!loadSeparatelySettingItem("sd:/wiisxrx/settings/", CdromId, false))
        {
            // If there is no separate setting
            // we load the common (global) settings.
            // Load common (global) settings from USB device
            if (!loadSeparatelySettingItem("usb:/wiisxrx/", "settingsRX2022", true))
            {
                // If there is no common (global) settings for USB
                // Load common (global) settings from SD card
                loadSeparatelySettingItem("sd:/wiisxrx/", "settingsRX2022", false);
            }
        }
    }

    // If the loadButton Slot changes, reload the key mapping
    if (oldLoadButtonSlot != loadButtonSlot)
    {
        // Load button mapping from USB
        if (!loadControllerMapping("usb"))
        {
            // If the key mapping in USB does not exist
            // Load button mapping from SD
            loadControllerMapping("sd");
        }
    }

    Config.pR3000Fix = 0;
    Config.Cpu = dynacore;

    // Init biosFile pointers and stuff
    biosFileInit();

    extern bool lightrec_mmap_inited;
    if (Config.Cpu == DYNACORE_DYNAREC && !lightrec_mmap_inited) // Lightrec
    {
        psxMemInit();
    }
    psxCpuInit();
    needInitCpu = true;

    // Setting the following values in OpenPlugins is incorrect because the CdromId has not been obtained yet
    // Only after Apply_Hacks_Cdrom was executed , dwActFixes and iUseDither set the correct values
    // Setting variables directly in the GPU is not good......
    extern uint32_t dwActFixes;
    extern int iUseDither;
    dwActFixes = Config.hacks.dwActFixes;
    iUseDither = useDithering;
}

// loadISO loads an ISO file as current media to read from.
int loadISOSwap(fileBrowser_file* file) {

  // Refresh file pointers
	memset(&isoFile, 0, sizeof(fileBrowser_file));
	memset(&cddaFile, 0, sizeof(fileBrowser_file));
	memset(&subFile, 0, sizeof(fileBrowser_file));

	memcpy(&isoFile, file, sizeof(fileBrowser_file) );

    CdromId[0] = '\0';
    CdromLabel[0] = '\0';
    cdrIsoMultidiskSelect++;

    CDR_close();
	//might need to insert code here to trigger a lid open/close interrupt
	if(CDR_open() < 0)
		return -1;

	CheckCdrom();

	loadSeparatelySetting();

	LoadCdrom();

	LidInterrupt();

	return 0;
}


// loadISO loads an ISO, resets the system and loads the save.
int loadISO(fileBrowser_file* file)
{
	// Refresh file pointers
	memset(&isoFile, 0, sizeof(fileBrowser_file));
	memset(&cddaFile, 0, sizeof(fileBrowser_file));
	memset(&subFile, 0, sizeof(fileBrowser_file));

	memcpy(&isoFile, file, sizeof(fileBrowser_file) );

	if(hasLoadedISO || executedBios) {
		SysClose();
		hasLoadedISO = FALSE;
	}
	needInitCpu = false;
	if(SysInit() < 0)
		return -1;
	hasLoadedISO = TRUE;

	char *tempStr = &file->name[0];
	if((strstr(tempStr,".EXE")!=NULL) || (strstr(tempStr,".exe")!=NULL)) {
		SysReset();
		Load(file);
	}
	else {
		CheckCdrom();

		loadSeparatelySetting();

		SysReset();
		LoadCdrom();
	}

	if(autoSave==AUTOSAVE_ENABLE) {
		switch (nativeSaveDevice)
		{
		case NATIVESAVEDEVICE_SD:
		case NATIVESAVEDEVICE_USB:
			// Adjust saveFile pointers
			saveFile_dir = (nativeSaveDevice==NATIVESAVEDEVICE_SD) ? &saveDir_libfat_Default:&saveDir_libfat_USB;
			saveFile_readFile  = fileBrowser_libfat_readFile;
			saveFile_writeFile = fileBrowser_libfat_writeFile;
			saveFile_init      = fileBrowser_libfat_init;
			saveFile_deinit    = fileBrowser_libfat_deinit;
			break;
		case NATIVESAVEDEVICE_CARDA:
		case NATIVESAVEDEVICE_CARDB:
			// Adjust saveFile pointers
			saveFile_dir       = (nativeSaveDevice==NATIVESAVEDEVICE_CARDA) ? &saveDir_CARD_SlotA:&saveDir_CARD_SlotB;
			saveFile_readFile  = fileBrowser_CARD_readFile;
			saveFile_writeFile = fileBrowser_CARD_writeFile;
			saveFile_init      = fileBrowser_CARD_init;
			saveFile_deinit    = fileBrowser_CARD_deinit;
			break;
		}
		// Try loading everything
		int result = 0;
		saveFile_init(saveFile_dir);
		result += LoadMcd(1,saveFile_dir);
		result += LoadMcd(2,saveFile_dir);
		saveFile_deinit(saveFile_dir);

		switch (nativeSaveDevice)
		{
		case NATIVESAVEDEVICE_SD:
			if (result) autoSaveLoaded = NATIVESAVEDEVICE_SD;
			break;
		case NATIVESAVEDEVICE_USB:
			if (result) autoSaveLoaded = NATIVESAVEDEVICE_USB;
			break;
		case NATIVESAVEDEVICE_CARDA:
			if (result) autoSaveLoaded = NATIVESAVEDEVICE_CARDA;
			break;
		case NATIVESAVEDEVICE_CARDB:
			if (result) autoSaveLoaded = NATIVESAVEDEVICE_CARDB;
			break;
		}
	}

	return 0;
}

void setOption(char* key, char* valuePointer){
	bool isString = valuePointer[0] == '"';
	char value = 0;

	if(isString) {
		char* p = valuePointer++;
		while(*++p != '"');
		*p = 0;
	} else
		value = atoi(valuePointer);

	for(unsigned int i=0; i<sizeof(OPTIONS)/sizeof(OPTIONS[0]); i++){
		if(!strcmp(OPTIONS[i].key, key)){
			if(isString) {
				if(OPTIONS[i].max == CONFIG_STRING_TYPE)
					strncpy(OPTIONS[i].value, valuePointer,
					        CONFIG_STRING_SIZE-1);
			} else if(value >= OPTIONS[i].min && value <= OPTIONS[i].max)
				*OPTIONS[i].value = value;
			break;
		}
	}
}

void handleConfigPair(char* kv){
	char* vs = kv;
	while(*vs != ' ' && *vs != '\t' && *vs != ':' && *vs != '=')
			++vs;
	*(vs++) = 0;
	while(*vs == ' ' || *vs == '\t' || *vs == ':' || *vs == '=')
			++vs;

	setOption(kv, vs);
}

void readConfig(FILE* f){
	char line[256];
	while(fgets(line, 256, f)){
		if(line[0] == '#') continue;
		handleConfigPair(line);
	}
}

void writeConfig(FILE* f){
	for(unsigned int i=0; i<sizeof(OPTIONS)/sizeof(OPTIONS[0]); ++i){
		if(OPTIONS[i].max == CONFIG_STRING_TYPE)
			fprintf(f, "%s = \"%s\"\n", OPTIONS[i].key, OPTIONS[i].value);
		else
			fprintf(f, "%s = %d\n", OPTIONS[i].key, *OPTIONS[i].value);
	}
}

extern "C" {
//System Functions
void go(void) {
	Config.PsxOut = 0;
	stop = 0;
	psxCpu->Execute();
}

int SysInit() {
#if defined (CPU_LOG) || defined(DMA_LOG) || defined(CDR_LOG) || defined(HW_LOG) || \
	defined(PSXBIOS_LOG) || defined(GTE_LOG) || defined(PAD_LOG)
	emuLog = fopen("sd:/wiisxrx/emu.log", "w");
#endif
	Config.Cpu = dynacore;  //cpu may have changed
	psxInit();
	LoadPlugins();
	if(OpenPlugins() < 0)
		return -1;

	return 0;
}

extern void pl_timing_prepare(int is_pal_);
int g_emu_resetting;

void SysReset() {
    g_emu_resetting = 1;
	// reset can run code, timing must be set
	pl_timing_prepare(Config.PsxType);

	psxReset();

	g_emu_resetting = 0;
}

void SysStartCPU() {
	Config.PsxOut = 0;
	stop = 0;
	psxCpu->Execute();
}

void SysClose()
{
	psxShutdown();
	ClosePlugins();
	ReleasePlugins();
#if defined (CPU_LOG) || defined(DMA_LOG) || defined(CDR_LOG) || defined(HW_LOG) || \
	defined(PSXBIOS_LOG) || defined(GTE_LOG) || defined(PAD_LOG)
	if (emuLog != NULL) fclose(emuLog);
#endif
}

void SysPrintf(const char *fmt, ...)
{
#ifdef PRINTGECKO
	va_list list;
	char msg[512];

	va_start(list, fmt);
	vsprintf(msg, fmt, list);
	va_end(list);

	//if (Config.PsxOut) printf ("%s", msg);
	DEBUG_print(msg, DBG_USBGECKO);
#if defined (CPU_LOG) || defined(DMA_LOG) || defined(CDR_LOG) || defined(HW_LOG) || \
	defined(PSXBIOS_LOG) || defined(GTE_LOG) || defined(PAD_LOG)
	fprintf(emuLog, "%s", msg);
#endif
#endif
}

void *SysLoadLibrary(char *lib)
{
	int i;
	for(i=0; i<NUM_PLUGINS; i++)
		if((plugins[i].lib != NULL) && (!strcmp(lib, plugins[i].lib)))
			return (void*)i;
	return NULL;
}

void *SysLoadSym(void *lib, char *sym)
{
	PluginTable* plugin = plugins + (int)lib;
	int i;
	for(i=0; i<plugin->numSyms; i++)
		if(plugin->syms[i].sym && !strcmp(sym, plugin->syms[i].sym))
			return plugin->syms[i].pntr;
	return NULL;
}

int framesdone = 0;
void SysUpdate()
{
	framesdone++;
	// reamed hack
	{
		extern void pl_frame_limit(void);
		pl_frame_limit();
	}
#ifdef PROFILE
	refresh_stat();
#endif
}

void SysRunGui() {}
void SysMessage(char *fmt, ...) {}
void SysCloseLibrary(void *lib) {}
char *SysLibError() {	return NULL; }

} //extern "C"
