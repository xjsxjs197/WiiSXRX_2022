/*  Pcsx - Pc Psx Emulator
 *  Copyright (C) 1999-2002  Pcsx Team
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

#ifndef GAMECUBE_PLUGINS_H
#define GAMECUBE_PLUGINS_H

#ifndef NULL
#define NULL ((void*)0)
#endif
#include "../decode_xa.h"
#include "../psemu_plugin_defs.h"
#include "../plugins.h"
#include "../gpulib/gpu.h"

#define SYMS_PER_LIB 32
typedef struct {
	const char* lib;
	int   numSyms;
	struct {
		const char* sym;
		void* pntr;
	} syms[SYMS_PER_LIB];
} PluginTable;
#define NUM_PLUGINS 8

/* SPU NULL */
/* dfsound */
void DF_SPUwriteRegister(unsigned long reg, unsigned short val, unsigned int cycles);
unsigned short DF_SPUreadRegister(unsigned long reg);
void DF_SPUreadDMAMem(unsigned short * pusPSXMem,int iSize, unsigned int cycles);
void DF_SPUwriteDMAMem(unsigned short * pusPSXMem,int iSize, unsigned int cycles);
void DF_SPUasync(unsigned long cycle, unsigned int flags, unsigned int psxType);
void DF_SPUplayADPCMchannel(xa_decode_t *xap, unsigned int cycle, int is_start);
int  DF_SPUplayCDDAchannel(short *pcm, int nbytes, unsigned int cycle, int is_start);
long DF_SPUinit(void);
long DF_SPUopen(void);
long DF_SPUclose(void);
long DF_SPUshutdown(void);
long DF_SPUfreeze(unsigned long ulFreezeMode,SPUFreeze_t * pF,uint32_t cycles);
long DF_SPUupdate(void);
void DF_SPUregisterCallback(void (*callback)(int));
void DF_SPUregisterScheduleCb(void (*callback)(unsigned int));
void DF_SPUsetCDvol(unsigned char ll, unsigned char lr,
        unsigned char rl, unsigned char rr, unsigned int cycle);

/* CDR */
long CDR__open(void);
long CDR__init(void);
long CDR__shutdown(void);
long CDR__open(void);
long CDR__close(void);
long CDR__getTN(unsigned char *);
long CDR__getTD(unsigned char , unsigned char *);
long CDR__readTrack(unsigned char *);
long CDR__play(unsigned char *sector);
long CDR__stop(void);
long CDR__getStatus(struct CdrStat *stat);
unsigned char *CDR__getBuffer(void);
unsigned char *CDR__getBufferSub(void);

/* NULL GPU */
//typedef long (* GPUopen)(unsigned long *, char *, char *);
long GPU__open(void);
long GPU__init(void);
long GPU__shutdown(void);
long GPU__close(void);
void GPU__writeStatus(unsigned long);
void GPU__writeData(unsigned long);
unsigned long GPU__readStatus(void);
unsigned long GPU__readData(void);
long GPU__dmaChain(unsigned long *,unsigned long);
void GPU__updateLace(void);

/* PAD */
//typedef long (* PADopen)(unsigned long *);
extern long PAD__init(long);
extern long PAD__shutdown(void);

/* WiiSX PAD Plugin */
extern long PAD__open(void);
extern long PAD__close(void);
unsigned char PAD__startPoll (int pad);
unsigned char PAD__poll (const unsigned char value);
long PAD__readPort1(PadDataS*);
long PAD__readPort2(PadDataS*);

/* SSSPSX PAD Plugin */
long SSS_PADopen (void *p);
long SSS_PADclose (void);
unsigned char SSS_PADstartPoll (int pad);
unsigned char SSS_PADpoll (const unsigned char value);
long SSS_PADreadPort1 (PadDataS* pads);
long SSS_PADreadPort2 (PadDataS* pads);

#define EMPTY_PLUGIN \
	{ NULL,      \
	  0,         \
	  { { NULL,  \
	      NULL }, } }

#define SSS_PAD1_PLUGIN \
	{ "PAD1",      \
	  7,         \
	  { { "PADinit",  \
	      (void*)PAD__init }, \
	    { "PADshutdown",	\
	      (void*)PAD__shutdown}, \
	    { "PADopen", \
	      (void*)SSS_PADopen}, \
	    { "PADclose", \
	      (void*)SSS_PADclose}, \
	    { "PADpoll", \
	      (void*)SSS_PADpoll}, \
	    { "PADstartPoll", \
	      (void*)SSS_PADstartPoll}, \
	    { "PADreadPort1", \
	      (void*)SSS_PADreadPort1} \
	       } }

#define SSS_PAD2_PLUGIN \
	{ "PAD2",      \
	  7,         \
	  { { "PADinit",  \
	      (void*)PAD__init }, \
	    { "PADshutdown",	\
	      (void*)PAD__shutdown}, \
	    { "PADopen", \
	      (void*)SSS_PADopen}, \
	    { "PADclose", \
	      (void*)SSS_PADclose}, \
	    { "PADpoll", \
	      (void*)SSS_PADpoll}, \
	    { "PADstartPoll", \
	      (void*)SSS_PADstartPoll}, \
	    { "PADreadPort2", \
	      (void*)SSS_PADreadPort2} \
	       } }

#define CDR_ISO_PLUGIN \
	{ "CDR",      \
	  12,         \
	  { { "CDRinit",  \
	      (void*)CDR_init }, \
	    { "CDRshutdown",	\
	      (void*)CDR_shutdown}, \
	    { "CDRopen", \
	      (void*)CDR_open}, \
	    { "CDRclose", \
	      (void*)CDR_close}, \
	    { "CDRgetTN", \
	      (void*)CDR_getTN}, \
	    { "CDRgetTD", \
	      (void*)CDR_getTD}, \
	    { "CDRreadTrack", \
	      (void*)CDR_readTrack}, \
	    { "CDRgetBuffer", \
	      (void*)CDR_getBuffer}, \
	    { "CDRplay", \
	      (void*)CDR_play}, \
	    { "CDRstop", \
	      (void*)CDR_stop}, \
	    { "CDRgetStatus", \
	      (void*)CDR_getStatus}, \
	    { "CDRgetBufferSub", \
	      (void*)CDR_getBufferSub} \
	       } }

#define DFSOUND_PLUGIN \
	{ "SPU",      \
	  15,         \
	  { { "SPUinit",  \
	      (void*)DF_SPUinit}, \
	    { "SPUshutdown", \
	      (void*)DF_SPUshutdown}, \
	    { "SPUopen", \
	      (void*)DF_SPUopen}, \
	    { "SPUclose", \
	      (void*)DF_SPUclose}, \
	    { "SPUwriteRegister", \
	      (void*)DF_SPUwriteRegister}, \
	    { "SPUreadRegister", \
	      (void*)DF_SPUreadRegister}, \
	    { "SPUwriteDMAMem", \
	      (void*)DF_SPUwriteDMAMem}, \
	    { "SPUreadDMAMem", \
	      (void*)DF_SPUreadDMAMem}, \
	    { "SPUplayADPCMchannel", \
	      (void*)DF_SPUplayADPCMchannel}, \
        { "SPUplayCDDAchannel", \
	      (void*)DF_SPUplayCDDAchannel}, \
	    { "SPUfreeze", \
	      (void*)DF_SPUfreeze}, \
	    { "SPUregisterCallback", \
	      (void*)DF_SPUregisterCallback}, \
        { "SPUregisterScheduleCb", \
	      (void*)DF_SPUregisterScheduleCb}, \
		{ "SPUsetCDvol", \
	      (void*)DF_SPUsetCDvol}, \
	    { "SPUasync", \
	      (void*)DF_SPUasync} \
	       } }

#define GPU_PEOPS_PLUGIN \
	{ "GPU",      \
	  14,         \
	  { { "GPUinit",  \
	      (void*)PEOPS_GPUinit }, \
	    { "GPUshutdown",	\
	      (void*)PEOPS_GPUshutdown}, \
	    { "GPUopen", \
	      (void*)PEOPS_GPUopen}, \
	    { "GPUclose", \
	      (void*)PEOPS_GPUclose}, \
	    { "GPUwriteStatus", \
	      (void*)PEOPS_GPUwriteStatus}, \
	    { "GPUwriteData", \
	      (void*)PEOPS_GPUwriteData}, \
	    { "GPUwriteDataMem", \
	      (void*)PEOPS_GPUwriteDataMem}, \
	    { "GPUreadStatus", \
	      (void*)PEOPS_GPUreadStatus}, \
	    { "GPUreadData", \
	      (void*)PEOPS_GPUreadData}, \
	    { "GPUreadDataMem", \
	      (void*)PEOPS_GPUreadDataMem}, \
	    { "GPUdmaChain", \
	      (void*)PEOPS_GPUdmaChain}, \
	    { "GPUdisplayText", \
	      (void*)PEOPS_GPUdisplayText}, \
	    { "GPUfreeze", \
	      (void*)PEOPS_GPUfreeze}, \
	    { "GPUupdateLace", \
	      (void*)PEOPS_GPUupdateLace} \
	       } }


#define GPU_SOFT_PLUGIN \
	{ "GPU",      \
	  14,         \
	  { { "GPUinit",  \
	      (void*)LIB_GPUinit }, \
	    { "GPUshutdown", \
	      (void*)LIB_GPUshutdown}, \
	    { "GPUopen", \
	      (void*)LIB_GPUopen}, \
	    { "GPUclose", \
	      (void*)LIB_GPUclose}, \
	    { "GPUwriteStatus", \
	      (void*)LIB_GPUwriteStatus}, \
	    { "GPUwriteData", \
	      (void*)LIB_GPUwriteData}, \
	    { "GPUwriteDataMem", \
	      (void*)LIB_GPUwriteDataMem}, \
	    { "GPUreadStatus", \
	      (void*)LIB_GPUreadStatus}, \
	    { "GPUreadData", \
	      (void*)LIB_GPUreadData}, \
	    { "GPUreadDataMem", \
	      (void*)LIB_GPUreadDataMem}, \
	    { "GPUdmaChain", \
	      (void*)LIB_GPUdmaChain}, \
	    { "GPUfreeze", \
	      (void*)LIB_GPUfreeze}, \
	    { "GPUupdateLace", \
	      (void*)LIB_GPUupdateLace}, \
		{ "GPUrearmedCallbacks", \
	      (void*)LIB_GPUrearmedCallbacks} \
	       } }


#define PLUGIN_SLOT_0 EMPTY_PLUGIN
#define PLUGIN_SLOT_1 SSS_PAD1_PLUGIN
#define PLUGIN_SLOT_2 SSS_PAD2_PLUGIN
#define PLUGIN_SLOT_3 CDR_ISO_PLUGIN
#define PLUGIN_SLOT_4 DFSOUND_PLUGIN
#define PLUGIN_SLOT_5 GPU_SOFT_PLUGIN
#define PLUGIN_SLOT_6 EMPTY_PLUGIN
#define PLUGIN_SLOT_7 EMPTY_PLUGIN



#endif

