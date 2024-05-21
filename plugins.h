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
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.           *
 ***************************************************************************/

#ifndef __PLUGINS_H__
#define __PLUGINS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "psxcommon.h"
#include "spu.h"

typedef void* HWND;
#define CALLBACK
typedef long (* GPUopen)(unsigned long *, char *, char *);
long GPU__open(void);
typedef long (* SPUopen)(void);
long SPU__open(void);
typedef long (* PADopen)(unsigned long *);
long PAD1__open(void);
long PAD2__open(void);
typedef long (* NETopen)(unsigned long *);

#include "psemu_plugin_defs.h"
#include "decode_xa.h"

int  LoadPlugins();
void ReleasePlugins();
int  OpenPlugins();
void ClosePlugins();


typedef unsigned long (CALLBACK* PSEgetLibType)(void);
typedef unsigned long (CALLBACK* PSEgetLibVersion)(void);
typedef char *(CALLBACK* PSEgetLibName)(void);

// CD-ROM Functions
typedef long (CALLBACK* CDRinit)(void);
typedef long (CALLBACK* CDRshutdown)(void);
typedef long (CALLBACK* CDRopen)(void);
typedef long (CALLBACK* CDRclose)(void);
typedef long (CALLBACK* CDRgetTN)(unsigned char *);
typedef long (CALLBACK* CDRgetTD)(unsigned char, unsigned char *);
typedef long (CALLBACK* CDRreadTrack)(unsigned char *);
typedef unsigned char* (CALLBACK* CDRgetBuffer)(void);
typedef unsigned char* (CALLBACK* CDRgetBufferSub)(int sector);
typedef long (CALLBACK* CDRconfigure)(void);
typedef long (CALLBACK* CDRtest)(void);
typedef void (CALLBACK* CDRabout)(void);
typedef long (CALLBACK* CDRplay)(unsigned char *);
typedef long (CALLBACK* CDRstop)(void);
typedef long (CALLBACK* CDRsetfilename)(char *);
struct CdrStat {
	uint32_t Type; // DATA, CDDA
	uint32_t Status; // same as cdr.StatP
	unsigned char Time_[3]; // unused
};
typedef long (CALLBACK* CDRgetStatus)(struct CdrStat *);
typedef char* (CALLBACK* CDRgetDriveLetter)(void);
struct SubQ {
	char res0[12];
	unsigned char ControlAndADR;
	unsigned char TrackNumber;
	unsigned char IndexNumber;
	unsigned char TrackRelativeAddress[3];
	unsigned char Filler;
	unsigned char AbsoluteAddress[3];
	unsigned char CRC[2];
	char res1[72];
};
typedef long (CALLBACK* CDRreadCDDA)(unsigned char, unsigned char, unsigned char, unsigned char *);
typedef long (CALLBACK* CDRgetTE)(unsigned char, unsigned char *, unsigned char *, unsigned char *);

// CD-ROM function pointers
extern CDRinit               CDR_init;
extern CDRshutdown           CDR_shutdown;
extern CDRopen               CDR_open;
extern CDRclose              CDR_close;
extern CDRtest               CDR_test;
extern CDRgetTN              CDR_getTN;
extern CDRgetTD              CDR_getTD;
extern CDRreadTrack          CDR_readTrack;
extern CDRgetBuffer          CDR_getBuffer;
extern CDRgetBufferSub       CDR_getBufferSub;
extern CDRplay               CDR_play;
extern CDRstop               CDR_stop;
extern CDRgetStatus          CDR_getStatus;
extern CDRgetDriveLetter     CDR_getDriveLetter;
extern CDRconfigure          CDR_configure;
extern CDRabout              CDR_about;
extern CDRsetfilename        CDR_setfilename;
extern CDRreadCDDA           CDR_readCDDA;
extern CDRgetTE              CDR_getTE;

long CALLBACK CDR__getStatus(struct CdrStat *stat);

// SPU Functions
typedef long (CALLBACK* SPUinit)(void);
typedef long (CALLBACK* SPUshutdown)(void);
typedef long (CALLBACK* SPUclose)(void);
typedef void (CALLBACK* SPUwriteRegister)(unsigned long, unsigned short, unsigned int);
typedef unsigned short (CALLBACK* SPUreadRegister)(unsigned long, unsigned int);
typedef void (CALLBACK* SPUwriteDMAMem)(unsigned short *, int, unsigned int);
typedef void (CALLBACK* SPUreadDMAMem)(unsigned short *, int, unsigned int);
typedef void (CALLBACK* SPUplayADPCMchannel)(xa_decode_t *, unsigned int, int);
typedef void (CALLBACK* SPUregisterCallback)(void (CALLBACK *callback)(int));
typedef void (CALLBACK* SPUregisterScheduleCb)(void (CALLBACK *callback)(unsigned int));
typedef struct
{
 char          szSPUName[8];
 uint32_t ulFreezeVersion;
 uint32_t ulFreezeSize;
 unsigned char cSPUPort[0x200];
 unsigned char cSPURam[0x80000];
 xa_decode_t   xaS;
} SPUFreeze_t;
typedef long (CALLBACK* SPUfreeze)(uint32_t, SPUFreeze_t *, uint32_t);
typedef void (CALLBACK* SPUasync)(uint32_t, uint32_t, uint32_t);
typedef int  (CALLBACK* SPUplayCDDAchannel)(short *, int, unsigned int, int);
typedef void (CALLBACK* SPUsetCDvol)(unsigned char, unsigned char, unsigned char, unsigned char, unsigned int);

// SPU function pointers
extern SPUinit             SPU_init;
extern SPUshutdown         SPU_shutdown;
extern SPUopen             SPU_open;
extern SPUclose            SPU_close;
extern SPUwriteRegister    SPU_writeRegister;
extern SPUreadRegister     SPU_readRegister;
extern SPUwriteDMAMem      SPU_writeDMAMem;
extern SPUreadDMAMem       SPU_readDMAMem;
extern SPUplayADPCMchannel SPU_playADPCMchannel;
extern SPUfreeze           SPU_freeze;
extern SPUregisterCallback SPU_registerCallback;
extern SPUregisterScheduleCb SPU_registerScheduleCb;
extern SPUasync            SPU_async;
extern SPUplayCDDAchannel  SPU_playCDDAchannel;
extern SPUsetCDvol         SPU_setCDvol;

// PAD Functions
typedef long (CALLBACK* PADconfigure)(void);
typedef void (CALLBACK* PADabout)(void);
typedef long (CALLBACK* PADinit)(long);
typedef long (CALLBACK* PADshutdown)(void);
typedef long (CALLBACK* PADtest)(void);
typedef long (CALLBACK* PADclose)(void);
typedef long (CALLBACK* PADquery)(void);
typedef long (CALLBACK*	PADreadPort1)(PadDataS*);
typedef long (CALLBACK* PADreadPort2)(PadDataS*);
typedef long (CALLBACK* PADkeypressed)(void);
typedef unsigned char (CALLBACK* PADstartPoll)(int);
typedef unsigned char (CALLBACK* PADpoll)(unsigned char);
typedef void (CALLBACK* PADsetSensitive)(int);

// PAD function pointers
extern PADconfigure        PAD1_configure;
extern PADabout            PAD1_about;
extern PADinit             PAD1_init;
extern PADshutdown         PAD1_shutdown;
extern PADtest             PAD1_test;
extern PADopen             PAD1_open;
extern PADclose            PAD1_close;
extern PADquery            PAD1_query;
extern PADreadPort1        PAD1_readPort1;
extern PADkeypressed       PAD1_keypressed;
extern PADstartPoll        PAD1_startPoll;
extern PADpoll             PAD1_poll;
extern PADsetSensitive     PAD1_setSensitive;

extern PADconfigure        PAD2_configure;
extern PADabout            PAD2_about;
extern PADinit             PAD2_init;
extern PADshutdown         PAD2_shutdown;
extern PADtest             PAD2_test;
extern PADopen             PAD2_open;
extern PADclose            PAD2_close;
extern PADquery            PAD2_query;
extern PADreadPort2        PAD2_readPort2;
extern PADkeypressed       PAD2_keypressed;
extern PADstartPoll        PAD2_startPoll;
extern PADpoll             PAD2_poll;
extern PADsetSensitive     PAD2_setSensitive;

// NET Functions
typedef long (CALLBACK* NETinit)(void);
typedef long (CALLBACK* NETshutdown)(void);
typedef long (CALLBACK* NETclose)(void);
typedef long (CALLBACK* NETconfigure)(void);
typedef long (CALLBACK* NETtest)(void);
typedef void (CALLBACK* NETabout)(void);
typedef void (CALLBACK* NETpause)(void);
typedef void (CALLBACK* NETresume)(void);
typedef long (CALLBACK* NETqueryPlayer)(void);
typedef long (CALLBACK* NETsendData)(void *, int, int);
typedef long (CALLBACK* NETrecvData)(void *, int, int);
typedef long (CALLBACK* NETsendPadData)(void *, int);
typedef long (CALLBACK* NETrecvPadData)(void *, int);

typedef struct {
	char EmuName[32];
	char CdromID[9];	// ie. 'SCPH12345', no \0 trailing character
	char CdromLabel[11];
	void *psxMem;
	PADsetSensitive PAD_setSensitive;
	char GPUpath[256];	// paths must be absolute
	char SPUpath[256];
	char CDRpath[256];
	char MCD1path[256];
	char MCD2path[256];
	char BIOSpath[256];	// 'HLE' for internal bios
	char Unused[1024];
} netInfo;

typedef long (CALLBACK* NETsetInfo)(netInfo *);
typedef long (CALLBACK* NETkeypressed)(int);


// NET function pointers
extern NETinit               NET_init;
extern NETshutdown           NET_shutdown;
extern NETopen               NET_open;
extern NETclose              NET_close;
extern NETtest               NET_test;
extern NETconfigure          NET_configure;
extern NETabout              NET_about;
extern NETpause              NET_pause;
extern NETresume             NET_resume;
extern NETqueryPlayer        NET_queryPlayer;
extern NETsendData           NET_sendData;
extern NETrecvData           NET_recvData;
extern NETsendPadData        NET_sendPadData;
extern NETrecvPadData        NET_recvPadData;
extern NETsetInfo            NET_setInfo;
extern NETkeypressed         NET_keypressed;

int LoadCDRplugin(char *CDRdll);
int LoadGPUplugin(char *GPUdll);
int LoadSPUplugin(char *SPUdll);
int LoadPAD1plugin(char *PAD1dll);
int LoadPAD2plugin(char *PAD2dll);
int LoadNETplugin(char *NETdll);

void CALLBACK clearDynarec(void);

#ifdef __cplusplus
}
#endif
#endif

