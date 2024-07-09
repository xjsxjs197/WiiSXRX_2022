/***************************************************************************
 *   Copyright (C) 2007 Ryan Schultz, PCSX-df Team, PCSX team              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02111-1307 USA.           *
 ***************************************************************************/

/*
* Plugin library callback/access functions.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static signed long long cdOpenCaseTime = 0;

#define EXT
#include "../psxcommon.h"
#include "GamecubePlugins.h"
#define CheckErr(func) \
    err = SysLibError(); \
    if (err != NULL) { SysPrintf("Error loading %s: %s\n", func, err); return -1; }

#define LoadSym(dest, src, name, checkerr) \
    dest = (src) SysLoadSym(drv, name); if (checkerr == 1) CheckErr(name); \
    if (checkerr == 2) { err = SysLibError(); if (err != NULL) errval = 1; }

CDRinit               CDR_init;
CDRshutdown           CDR_shutdown;
CDRopen               CDR_open;
CDRclose              CDR_close;
CDRtest               CDR_test;
CDRgetTN              CDR_getTN;
CDRgetTD              CDR_getTD;
CDRreadTrack          CDR_readTrack;
CDRgetBuffer          CDR_getBuffer;
CDRplay               CDR_play;
CDRstop               CDR_stop;
CDRgetStatus          CDR_getStatus;
CDRgetDriveLetter     CDR_getDriveLetter;
CDRgetBufferSub       CDR_getBufferSub;
CDRconfigure          CDR_configure;
CDRabout              CDR_about;
CDRsetfilename        CDR_setfilename;
CDRreadCDDA           CDR_readCDDA;
CDRgetTE              CDR_getTE;

SPUinit               SPU_init;
SPUshutdown           SPU_shutdown;
SPUopen               SPU_open;
SPUclose              SPU_close;
SPUwriteRegister      SPU_writeRegister;
SPUreadRegister       SPU_readRegister;
SPUwriteDMAMem        SPU_writeDMAMem;
SPUreadDMAMem         SPU_readDMAMem;
SPUplayADPCMchannel   SPU_playADPCMchannel;
SPUfreeze             SPU_freeze;
SPUregisterCallback   SPU_registerCallback;
SPUregisterScheduleCb SPU_registerScheduleCb;
SPUasync              SPU_async;
SPUplayCDDAchannel    SPU_playCDDAchannel;
SPUsetCDvol           SPU_setCDvol;

PADconfigure          PAD1_configure;
PADabout              PAD1_about;
PADinit               PAD1_init;
PADshutdown           PAD1_shutdown;
PADtest               PAD1_test;
PADopen               PAD1_open;
PADclose              PAD1_close;
PADquery              PAD1_query;
PADreadPort1          PAD1_readPort1;
PADkeypressed         PAD1_keypressed;
PADstartPoll          PAD1_startPoll;
PADpoll               PAD1_poll;
PADsetSensitive       PAD1_setSensitive;

PADconfigure          PAD2_configure;
PADabout              PAD2_about;
PADinit               PAD2_init;
PADshutdown           PAD2_shutdown;
PADtest               PAD2_test;
PADopen               PAD2_open;
PADclose              PAD2_close;
PADquery              PAD2_query;
PADreadPort2          PAD2_readPort2;
PADkeypressed         PAD2_keypressed;
PADstartPoll          PAD2_startPoll;
PADpoll               PAD2_poll;
PADsetSensitive       PAD2_setSensitive;

NETinit               NET_init;
NETshutdown           NET_shutdown;
NETopen               NET_open;
NETclose              NET_close;
NETtest               NET_test;
NETconfigure          NET_configure;
NETabout              NET_about;
NETpause              NET_pause;
NETresume             NET_resume;
NETqueryPlayer        NET_queryPlayer;
NETsendData           NET_sendData;
NETrecvData           NET_recvData;
NETsendPadData        NET_sendPadData;
NETrecvPadData        NET_recvPadData;
NETsetInfo            NET_setInfo;
NETkeypressed         NET_keypressed;
static const char *err;
static int errval;
void *hGPUDriver;

void ConfigurePlugins();


void CALLBACK GPU__makeSnapshot(void) {}
void CALLBACK GPU__keypressed(int key) {}
long CALLBACK GPU__getScreenPic(unsigned char *pMem) { return -1; }
long CALLBACK GPU__showScreenPic(unsigned char *pMem) { return -1; }
void CALLBACK GPU__vBlank(int val) {}
void CALLBACK GPU__getScreenInfo(int *y, int *base_hres) {}

#define LoadGpuSym1(dest, name) \
	LoadSym(GPU_##dest, GPU##dest, name, 1);

#define LoadGpuSym0(dest, name) \
	LoadSym(GPU_##dest, GPU##dest, name, 0); \
	if (GPU_##dest == NULL) GPU_##dest = (GPU##dest) GPU__##dest;

#define LoadGpuSymN(dest, name) \
	LoadSym(GPU_##dest, GPU##dest, name, 0);

gpu_t *gpuPtr;
int LoadGPUplugin(char *GPUdll) {
	//gpuPtr = &oldSoftGpu; // There's no need to set it up again here

	return 0;
}

void *hCDRDriver;
long CALLBACK CDR__play(unsigned char *sector);
long CALLBACK CDR__stop(void);

// reads cdr status
// type:
// 0x00 - unknown
// 0x01 - data
// 0x02 - audio
// 0xff - no cdrom
// status:
// 0x00 - unknown
// 0x02 - error
// 0x08 - seek error
// 0x10 - shell open
// 0x20 - reading
// 0x40 - seeking
// 0x80 - playing
// time:
// byte 0 - minute
// byte 1 - second
// byte 2 - frame
long CALLBACK CDR__getStatus(struct CdrStat *stat) {
	if (cdOpenCaseTime < 0 || cdOpenCaseTime > (s64)time(NULL))
		stat->Status = 0x10;
	else
		stat->Status = 0;

//  if(isCDDAPlaying) {
//    stat->Type = 0x02;    // Audio
//    stat->Status|=0x80;   // Playing flag
//    // Time will need to be updated in a thread.
//    stat->Time[0] = 0;  // current play time
//    stat->Time[1] = 0;
//    stat->Time[2] = 0;
//  }
//  else {
//    stat->Type = 0x01;    // Data
//  }

	return 0;
}

char* CALLBACK CDR__getDriveLetter(void) { return NULL; }

#define LoadCdrSym1(dest, name) \
	LoadSym(CDR_##dest, CDR##dest, name, 1);

#define LoadCdrSym0(dest, name) \
	LoadSym(CDR_##dest, CDR##dest, name, 0); \
	if (CDR_##dest == NULL) CDR_##dest = (CDR##dest) CDR__##dest;

#define LoadCdrSymN(dest, name) \
	LoadSym(CDR_##dest, CDR##dest, name, 0);

int LoadCDRplugin(char *CDRdll) {
    if (true) {
		cdrIsoInit();
		return 0;
	}

	void *drv;

	hCDRDriver = SysLoadLibrary(CDRdll);
	if (hCDRDriver == NULL) {
		SysPrintf ("Could Not load CDR plugin %s\n", CDRdll);  return -1;
	}
	drv = hCDRDriver;
	LoadCdrSym1(init, "CDRinit");
	LoadCdrSym1(shutdown, "CDRshutdown");
	LoadCdrSym1(open, "CDRopen");
	LoadCdrSym1(close, "CDRclose");
	LoadCdrSym1(getTN, "CDRgetTN");
	LoadCdrSym1(getTD, "CDRgetTD");
	LoadCdrSym1(readTrack, "CDRreadTrack");
	LoadCdrSym1(getBuffer, "CDRgetBuffer");
	LoadCdrSym1(getBufferSub, "CDRgetBufferSub");
	LoadCdrSym0(play, "CDRplay");
	LoadCdrSym0(stop, "CDRstop");
	LoadCdrSym0(getStatus, "CDRgetStatus");
	LoadCdrSym0(getDriveLetter, "CDRgetDriveLetter");
    LoadCdrSymN(readCDDA, "CDRreadCDDA");

	return 0;
}

void *hSPUDriver;

#define LoadSpuSym1(dest, name) \
	LoadSym(SPU_##dest, SPU##dest, name, 1);

#define LoadSpuSym2(dest, name) \
	LoadSym(SPU_##dest, SPU##dest, name, 2);

#define LoadSpuSym0(dest, name) \
	LoadSym(SPU_##dest, SPU##dest, name, 0); \
	if (SPU_##dest == NULL) SPU_##dest = (SPU##dest) SPU__##dest;

#define LoadSpuSymE(dest, name) \
	LoadSym(SPU_##dest, SPU##dest, name, errval); \
	if (SPU_##dest == NULL) SPU_##dest = (SPU##dest) SPU__##dest;

#define LoadSpuSymN(dest, name) \
	LoadSym(SPU_##dest, SPU##dest, name, 0); \

int LoadSPUplugin(char *SPUdll) {
	void *drv;

	hSPUDriver = SysLoadLibrary(SPUdll);
	if (hSPUDriver == NULL) {
		SysPrintf ("Could not open SPU plugin %s\n", SPUdll); return -1;
	}
	drv = hSPUDriver;
	LoadSpuSym1(init, "SPUinit");
	LoadSpuSym1(shutdown, "SPUshutdown");
	LoadSpuSym1(open, "SPUopen");
	LoadSpuSym1(close, "SPUclose");
	errval = 0;
	LoadSpuSym1(writeRegister, "SPUwriteRegister");
	LoadSpuSym1(readRegister, "SPUreadRegister");
	LoadSpuSym1(writeDMAMem, "SPUwriteDMAMem");
	LoadSpuSym1(readDMAMem, "SPUreadDMAMem");
	LoadSpuSym1(playADPCMchannel, "SPUplayADPCMchannel");
	LoadSpuSym1(freeze, "SPUfreeze");
	LoadSpuSym1(registerCallback, "SPUregisterCallback");
	LoadSpuSym1(registerScheduleCb, "SPUregisterScheduleCb");
	LoadSpuSymN(async, "SPUasync");
	LoadSpuSymN(playCDDAchannel, "SPUplayCDDAchannel");
	LoadSpuSym1(setCDvol, "SPUsetCDvol");

	return 0;
}


void *hPAD1Driver;
void *hPAD2Driver;
/*
static unsigned char buf[256];
unsigned char stdpar[10] = { 0x00, 0x41, 0x5a, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
unsigned char mousepar[8] = { 0x00, 0x12, 0x5a, 0xff, 0xff, 0xff, 0xff };
unsigned char analogpar[9] = { 0x00, 0xff, 0x5a, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

static int bufcount, bufc;

PadDataS padd1, padd2;

unsigned char _PADstartPoll(PadDataS *pad) {
	bufc = 0;

	switch (pad->controllerType) {
		case PSE_PAD_TYPE_MOUSE:
			mousepar[3] = pad->buttonStatus & 0xff;
			mousepar[4] = pad->buttonStatus >> 8;
			mousepar[5] = pad->moveX;
			mousepar[6] = pad->moveY;

			memcpy(buf, mousepar, 7);
			bufcount = 6;
			break;
		case PSE_PAD_TYPE_NEGCON: // npc101/npc104(slph00001/slph00069)
			analogpar[1] = 0x23;
			analogpar[3] = pad->buttonStatus & 0xff;
			analogpar[4] = pad->buttonStatus >> 8;
			analogpar[5] = pad->rightJoyX;
			analogpar[6] = pad->rightJoyY;
			analogpar[7] = pad->leftJoyX;
			analogpar[8] = pad->leftJoyY;

			memcpy(buf, analogpar, 9);
			bufcount = 8;
			break;
		case PSE_PAD_TYPE_ANALOGPAD: // scph1150
			analogpar[1] = 0x73;
			analogpar[3] = pad->buttonStatus & 0xff;
			analogpar[4] = pad->buttonStatus >> 8;
			analogpar[5] = pad->rightJoyX;
			analogpar[6] = pad->rightJoyY;
			analogpar[7] = pad->leftJoyX;
			analogpar[8] = pad->leftJoyY;

			memcpy(buf, analogpar, 9);
			bufcount = 8;
			break;
		case PSE_PAD_TYPE_ANALOGJOY: // scph1110
			analogpar[1] = 0x53;
			analogpar[3] = pad->buttonStatus & 0xff;
			analogpar[4] = pad->buttonStatus >> 8;
			analogpar[5] = pad->rightJoyX;
			analogpar[6] = pad->rightJoyY;
			analogpar[7] = pad->leftJoyX;
			analogpar[8] = pad->leftJoyY;

			memcpy(buf, analogpar, 9);
			bufcount = 8;
			break;
		case PSE_PAD_TYPE_STANDARD:
		default:
			stdpar[3] = pad->buttonStatus & 0xff;
			stdpar[4] = pad->buttonStatus >> 8;

			memcpy(buf, stdpar, 5);
			bufcount = 4;
			break;
	}

	return buf[bufc++];
}

unsigned char _PADpoll(unsigned char value) {
	if (bufc > bufcount) return 0;
	return buf[bufc++];
}

unsigned char CALLBACK PAD1__startPoll(int pad) {
	PadDataS padd;

	PAD1_readPort1(&padd);

	return _PADstartPoll(&padd);
}

unsigned char CALLBACK PAD1__poll(unsigned char value) {
	return _PADpoll(value);
}*/

long CALLBACK PAD1__configure(void) { return 0; }
void CALLBACK PAD1__about(void) {}
long CALLBACK PAD1__test(void) { return 0; }
long CALLBACK PAD1__query(void) { return 3; }
long CALLBACK PAD1__keypressed() { return 0; }

#define LoadPad1Sym1(dest, name) \
	LoadSym(PAD1_##dest, PAD##dest, name, 1);

#define LoadPad1SymN(dest, name) \
	LoadSym(PAD1_##dest, PAD##dest, name, 0);

#define LoadPad1Sym0(dest, name) \
	LoadSym(PAD1_##dest, PAD##dest, name, 0); \
	if (PAD1_##dest == NULL) PAD1_##dest = (PAD##dest) PAD1__##dest;

int LoadPAD1plugin(char *PAD1dll) {
	void *drv;

	hPAD1Driver = SysLoadLibrary(PAD1dll);
	if (hPAD1Driver == NULL) {
		PAD1_configure = NULL;
		SysPrintf ("Could Not load PAD1 plugin %s\n", PAD1dll); return -1;
	}
	drv = hPAD1Driver;
	LoadPad1Sym1(init, "PADinit");
	LoadPad1Sym1(shutdown, "PADshutdown");
	LoadPad1Sym1(open, "PADopen");
	LoadPad1Sym1(close, "PADclose");
	LoadPad1Sym0(query, "PADquery");
	LoadPad1Sym1(readPort1, "PADreadPort1");
	LoadPad1Sym0(configure, "PADconfigure");
	LoadPad1Sym0(test, "PADtest");
	LoadPad1Sym0(about, "PADabout");
	LoadPad1Sym0(keypressed, "PADkeypressed");
	LoadPad1Sym1(startPoll, "PADstartPoll");
	LoadPad1Sym1(poll, "PADpoll");
	LoadPad1SymN(setSensitive, "PADsetSensitive");

	return 0;
}
/*
unsigned char CALLBACK PAD2__startPoll(int pad) {
	PadDataS padd;

	PAD2_readPort2(&padd);

	return _PADstartPoll(&padd);
}

unsigned char CALLBACK PAD2__poll(unsigned char value) {
	return _PADpoll(value);
}
*/
long CALLBACK PAD2__configure(void) { return 0; }
void CALLBACK PAD2__about(void) {}
long CALLBACK PAD2__test(void) { return 0; }
long CALLBACK PAD2__query(void) { return 3; }
long CALLBACK PAD2__keypressed() { return 0; }

#define LoadPad2Sym1(dest, name) \
	LoadSym(PAD2_##dest, PAD##dest, name, 1);

#define LoadPad2Sym0(dest, name) \
	LoadSym(PAD2_##dest, PAD##dest, name, 0); \
	if (PAD2_##dest == NULL) PAD2_##dest = (PAD##dest) PAD2__##dest;

#define LoadPad2SymN(dest, name) \
	LoadSym(PAD2_##dest, PAD##dest, name, 0);

int LoadPAD2plugin(char *PAD2dll) {
	void *drv;

	hPAD2Driver = SysLoadLibrary(PAD2dll);
	if (hPAD2Driver == NULL) {
		PAD2_configure = NULL;
		SysPrintf ("Could Not load PAD plugin %s\n", PAD2dll); return -1;
	}
	drv = hPAD2Driver;
	LoadPad2Sym1(init, "PADinit");
	LoadPad2Sym1(shutdown, "PADshutdown");
	LoadPad2Sym1(open, "PADopen");
	LoadPad2Sym1(close, "PADclose");
	LoadPad2Sym0(query, "PADquery");
	LoadPad2Sym1(readPort2, "PADreadPort2");
	LoadPad2Sym0(configure, "PADconfigure");
	LoadPad2Sym0(test, "PADtest");
	LoadPad2Sym0(about, "PADabout");
	LoadPad2Sym0(keypressed, "PADkeypressed");
	LoadPad2Sym1(startPoll, "PADstartPoll");
	LoadPad2Sym1(poll, "PADpoll");
	LoadPad2SymN(setSensitive, "PADsetSensitive");

	return 0;
}

void *hNETDriver;

void CALLBACK NET__setInfo(netInfo *info) {}
void CALLBACK NET__keypressed(int key) {}
long CALLBACK NET__configure(void) { return 0; }
long CALLBACK NET__test(void) { return 0; }
void CALLBACK NET__about(void) {}

#define LoadNetSym1(dest, name) \
	LoadSym(NET_##dest, NET##dest, name, 1);

#define LoadNetSymN(dest, name) \
	LoadSym(NET_##dest, NET##dest, name, 0);

#define LoadNetSym0(dest, name) \
	LoadSym(NET_##dest, NET##dest, name, 0); \
	if (NET_##dest == NULL) NET_##dest = (NET##dest) NET__##dest;

int LoadNETplugin(char *NETdll) {
	void *drv;

	hNETDriver = SysLoadLibrary(NETdll);
	if (hNETDriver == NULL) {
		SysPrintf ("Could Not load NET plugin %s\n", NETdll); return -1;
	}
	drv = hNETDriver;
	LoadNetSym1(init, "NETinit");
	LoadNetSym1(shutdown, "NETshutdown");
	LoadNetSym1(open, "NETopen");
	LoadNetSym1(close, "NETclose");
	LoadNetSymN(sendData, "NETsendData");
	LoadNetSymN(recvData, "NETrecvData");
	LoadNetSym1(sendPadData, "NETsendPadData");
	LoadNetSym1(recvPadData, "NETrecvPadData");
	LoadNetSym1(queryPlayer, "NETqueryPlayer");
	LoadNetSym1(pause, "NETpause");
	LoadNetSym1(resume, "NETresume");
	LoadNetSym0(setInfo, "NETsetInfo");
	LoadNetSym0(keypressed, "NETkeypressed");
	LoadNetSym0(configure, "NETconfigure");
	LoadNetSym0(test, "NETtest");
	LoadNetSym0(about, "NETabout");

	return 0;
}

void CALLBACK clearDynarec(void) {
	psxCpu->Reset();
}

int LoadPlugins() {

	if (LoadCDRplugin("CDR") == -1) return -1;
	if (LoadGPUplugin("GPU") == -1) return -1;
	if (LoadSPUplugin("SPU") == -1) return -1;
	if (LoadPAD1plugin("PAD1") == -1) return -1;
	if (LoadPAD2plugin("PAD2") == -1) return -1;

	if (!strcmp("Disabled", Config.Net)) Config.UseNet = 0;
	else {
		char Plugin[256];
		Config.UseNet = 1;
		sprintf(Plugin, "%s%s", Config.PluginsDir, Config.Net);
		if (LoadNETplugin(Plugin) == -1) return -1;
	}

#ifndef __MACOSX__
	int ret = CDR_init();
	if (ret < 0) { SysPrintf ("CDRinit error : %d\n", ret); return -1; }
	ret = gpuPtr->init();
	if (ret < 0) { SysPrintf ("GPUinit error: %d\n", ret); return -1; }
	ret = SPU_init();
	if (ret < 0) { SysPrintf ("SPUinit error: %d\n", ret); return -1; }
	ret = PAD1_init(1);
	if (ret < 0) { SysPrintf ("PAD1init error: %d\n", ret); return -1; }
	ret = PAD2_init(2);
	if (ret < 0) { SysPrintf ("PAD2init error: %d\n", ret); return -1; }
	if (Config.UseNet) {
		ret = NET_init();
		if (ret < 0) { SysPrintf ("NETinit error: %d\n", ret); return -1; }
	}
#endif

	return 0;
}

void ReleasePlugins() {
	if (hCDRDriver  == NULL || hSPUDriver == NULL ||
		hPAD1Driver == NULL || hPAD2Driver == NULL) return;

	if (Config.UseNet) {
		int ret = NET_close();
		if (ret < 0) Config.UseNet = 0;
		NetOpened = 0;
	}

#ifndef __MACOSX__
	CDR_shutdown();
	gpuPtr->shutdown();
	SPU_shutdown();
	PAD1_shutdown();
	PAD2_shutdown();
	if (Config.UseNet && hNETDriver != NULL) NET_shutdown();
#endif

	SysCloseLibrary(hCDRDriver); hCDRDriver = NULL;
	//SysCloseLibrary(hGPUDriver); hGPUDriver = NULL;
	SysCloseLibrary(hSPUDriver); hSPUDriver = NULL;
	SysCloseLibrary(hPAD1Driver); hPAD1Driver = NULL;
	SysCloseLibrary(hPAD2Driver); hPAD2Driver = NULL;
	if (Config.UseNet && hNETDriver != NULL) {
		SysCloseLibrary(hNETDriver); hNETDriver = NULL;
	}
}

bool UsingIso(void) {
	return (&isoFile.name[0] != '\0');
}

// for CD swap
int ReloadCdromPlugin()
{
	if (hCDRDriver != NULL || cdrIsoActive()) CDR_shutdown();
	if (hCDRDriver != NULL) { SysCloseLibrary(hCDRDriver); hCDRDriver = NULL; }

	if (UsingIso()) {
		LoadCDRplugin(NULL);
	} else {
		char Plugin[MAXPATHLEN * 2];
		sprintf(Plugin, "%s/%s", Config.PluginsDir, Config.Cdr);
		if (LoadCDRplugin(Plugin) == -1) return -1;
	}

	return CDR_init();
}

void SetIsoFile(const char *filename) {
	if (filename == NULL) {
		isoFile.name[0] = '\0';
		return;
	}
	strncpy(isoFile.name, filename, FILE_BROWSER_MAX_PATH_LEN - 1);
}

void SetCdOpenCaseTime(s64 time) {
	cdOpenCaseTime = time;
}
