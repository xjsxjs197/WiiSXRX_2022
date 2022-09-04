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

/*
* Handles all CD-ROM registers and functions.
*/

#include "cdrom.h"
#include "ppf.h"
#include "psxdma.h"
#include <ogc/lwp_watchdog.h>
#include "Gamecube/DEBUG.h"
/* logging */
#if 0
#define CDR_LOG SysPrintf
#else
#define CDR_LOG(...)
#endif
#if 0
#define CDR_LOG_I SysPrintf
#else
#define CDR_LOG_I(...)
#endif
#if 0
#define CDR_LOG_IO SysPrintf
#else
#define CDR_LOG_IO(...)
#endif
//#define CDR_LOG_CMD_IRQ

cdrStruct cdr;
static unsigned char *pTransfer;
static s16 read_buf[CD_FRAMESIZE_RAW/2];

/* CD-ROM magic numbers */
#define CdlSync        0 /* nocash documentation : "Uh, actually, returns error code 40h = Invalid Command...?" */
#define CdlNop         1
#define CdlSetloc      2
#define CdlPlay        3
#define CdlForward     4
#define CdlBackward    5
#define CdlReadN       6
#define CdlStandby     7
#define CdlStop        8
#define CdlPause       9
#define CdlReset       10
#define CdlMute        11
#define CdlDemute      12
#define CdlSetfilter   13
#define CdlSetmode     14
#define CdlGetparam    15
#define CdlGetlocL     16
#define CdlGetlocP     17
#define CdlReadT       18
#define CdlGetTN       19
#define CdlGetTD       20
#define CdlSeekL       21
#define CdlSeekP       22
#define CdlSetclock    23
#define CdlGetclock    24
#define CdlTest        25
#define CdlID          26
#define CdlReadS       27
#define CdlInit        28
#define CdlGetQ        29
#define CdlReadToc     30

char *CmdName[0x100]= {
    "CdlSync",     "CdlNop",       "CdlSetloc",  "CdlPlay",
    "CdlForward",  "CdlBackward",  "CdlReadN",   "CdlStandby",
    "CdlStop",     "CdlPause",     "CdlReset",   "CdlMute",
    "CdlDemute",   "CdlSetfilter", "CdlSetmode", "CdlGetparam",
    "CdlGetlocL",  "CdlGetlocP",   "CdlReadT",   "CdlGetTN",
    "CdlGetTD",    "CdlSeekL",     "CdlSeekP",   "CdlSetclock",
    "CdlGetclock", "CdlTest",      "CdlID",      "CdlReadS",
    "CdlInit",    NULL,           "CDlReadToc", NULL
};

unsigned char Test04[] = { 0 };
unsigned char Test05[] = { 0 };
unsigned char Test20[] = { 0x98, 0x06, 0x10, 0xC3 };
unsigned char Test22[] = { 0x66, 0x6F, 0x72, 0x20, 0x45, 0x75, 0x72, 0x6F };
unsigned char Test23[] = { 0x43, 0x58, 0x44, 0x32, 0x39 ,0x34, 0x30, 0x51 };

unsigned char btoiBuf[] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,
    20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,40,41,42,43,
    44,45,46,47,48,49,50,51,52,53,54,55,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,60,61,62,63,64,65,66,67,
    68,69,70,71,72,73,74,75,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,80,81,82,83,84,85,86,87,88,89,90,91,
    92,93,94,95,90,91,92,93,94,95,96,97,98,99,100,101,102,103,104,105,100,101,102,103,104,105,106,107,108,109,110,
    111,112,113,114,115,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,120,121,122,123,124,125,126,
    127,128,129,130,131,132,133,134,135,130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,145,140,141,142,
    143,144,145,146,147,148,149,150,151,152,153,154,155,150,151,152,153,154,155,156,157,158,159,160,161,162,163,164,165
};
unsigned char itobBuf[] = {0,1,2,3,4,5,6,7,8,9,16,17,18,19,20,21,22,23,24,25,32,33,34,35,36,37,38,39,40,41,48,49,50,
    51,52,53,54,55,56,57,64,65,66,67,68,69,70,71,72,73,80,81,82,83,84,85,86,87,88,89,96,97,98,99,100,101,102,103,104,
    105,112,113,114,115,116,117,118,119,120,121,128,129,130,131,132,133,134,135,136,137,144,145,146,147,148,149,150,
    151,152,153,160,161,162,163,164,165,166,167,168,169,176,177,178,179,180,181,182,183,184,185,192,193,194,195,196,
    197,198,199,200,201,208,209,210,211,212,213,214,215,216,217,224,225,226,227,228,229,230,231,232,233,240,241,242,
    243,244,245,246,247,248,249,0,1,2,3,4,5,6,7,8,9,16,17,18,19,20,21,22,23,24,25,32,33,34,35,36,37,38,39,40,41,48,49,
    50,51,52,53,54,55,56,57,64,65,66,67,68,69,70,71,72,73,80,81,82,83,84,85,86,87,88,89,96,97,98,99,100,101,102,103,104,
    105,112,113,114,115,116,117,118,119,120,121,128,129,130,131,132,133,134,135,136,137,144,145,146,147,148,149
};

int msf2SectM[] = {
    0,4500,9000,13500,18000,22500,27000,31500,36000,40500,45000,49500,54000,58500,63000,67500,45000,49500,54000,58500,63000,67500,72000,76500,81000,85500,90000,
    94500,99000,103500,108000,112500,90000,94500,99000,103500,108000,112500,117000,121500,126000,130500,135000,139500,144000,148500,153000,157500,135000,139500,
    144000,148500,153000,157500,162000,166500,171000,175500,180000,184500,189000,193500,198000,202500,180000,184500,189000,193500,198000,202500,207000,211500,216000,
    220500,225000,229500,234000,238500,243000,247500,225000,229500,234000,238500,243000,247500,252000,256500,261000,265500,270000,274500,279000,283500,288000,292500,
    270000,274500,279000,283500,288000,292500,297000,301500,306000,310500,315000,319500,324000,328500,333000,337500,315000,319500,324000,328500,333000,337500,342000,
    346500,351000,355500,360000,364500,369000,373500,378000,382500,360000,364500,369000,373500,378000,382500,387000,391500,396000,400500,405000,409500,414000,418500,
    423000,427500,405000,409500,414000,418500,423000,427500,432000,436500,441000,445500,450000,454500,459000,463500,468000,472500,450000,454500,459000,463500,468000,
    472500,477000,481500,486000,490500,495000,499500,504000,508500,513000,517500,495000,499500,504000,508500,513000,517500,522000,526500,531000,535500,540000,544500,
    549000,553500,558000,562500,540000,544500,549000,553500,558000,562500,567000,571500,576000,580500,585000,589500,594000,598500,603000,607500,585000,589500,594000,
    598500,603000,607500,612000,616500,621000,625500,630000,634500,639000,643500,648000,652500,630000,634500,639000,643500,648000,652500,657000,661500,666000,670500,
    675000,679500,684000,688500,693000,697500,675000,679500,684000,688500,693000,697500,702000,706500,711000,715500,720000,724500,729000,733500,738000,742500
};

int msf2SectS[] = {
    0,75,150,225,300,375,450,525,600,675,750,825,900,975,1050,1125,750,825,900,975,1050,1125,1200,1275,1350,1425,1500,1575,1650,1725,1800,1875,1500,1575,1650,1725,
    1800,1875,1950,2025,2100,2175,2250,2325,2400,2475,2550,2625,2250,2325,2400,2475,2550,2625,2700,2775,2850,2925,3000,3075,3150,3225,3300,3375,3000,3075,3150,3225,
    3300,3375,3450,3525,3600,3675,3750,3825,3900,3975,4050,4125,3750,3825,3900,3975,4050,4125,4200,4275,4350,4425,4500,4575,4650,4725,4800,4875,4500,4575,4650,4725,
    4800,4875,4950,5025,5100,5175,5250,5325,5400,5475,5550,5625,5250,5325,5400,5475,5550,5625,5700,5775,5850,5925,6000,6075,6150,6225,6300,6375,6000,6075,6150,6225,
    6300,6375,6450,6525,6600,6675,6750,6825,6900,6975,7050,7125,6750,6825,6900,6975,7050,7125,7200,7275,7350,7425,7500,7575,7650,7725,7800,7875,7500,7575,7650,7725,
    7800,7875,7950,8025,8100,8175,8250,8325,8400,8475,8550,8625,8250,8325,8400,8475,8550,8625,8700,8775,8850,8925,9000,9075,9150,9225,9300,9375,9000,9075,9150,9225,
    9300,9375,9450,9525,9600,9675,9750,9825,9900,9975,10050,10125,9750,9825,9900,9975,10050,10125,10200,10275,10350,10425,10500,10575,10650,10725,10800,10875,10500,
    10575,10650,10725,10800,10875,10950,11025,11100,11175,11250,11325,11400,11475,11550,11625,11250,11325,11400,11475,11550,11625,11700,11775,11850,11925,12000,12075,
    12150,12225,12300,12375
};

// cdr.Stat:
#define NoIntr		0
#define DataReady	1
#define Complete	2
#define Acknowledge	3
#define DataEnd		4
#define DiskError	5

/* Modes flags */
#define MODE_SPEED       (1<<7) // 0x80
#define MODE_STRSND      (1<<6) // 0x40 ADPCM on/off
#define MODE_SIZE_2340   (1<<5) // 0x20
#define MODE_SIZE_2328   (1<<4) // 0x10
#define MODE_SIZE_2048   (0<<4) // 0x00
#define MODE_SF          (1<<3) // 0x08 channel on/off
#define MODE_REPORT      (1<<2) // 0x04
#define MODE_AUTOPAUSE   (1<<1) // 0x02
#define MODE_CDDA        (1<<0) // 0x01

/* Status flags */
#define STATUS_PLAY      (1<<7) // 0x80
#define STATUS_SEEK      (1<<6) // 0x40
#define STATUS_READ      (1<<5) // 0x20
#define STATUS_SHELLOPEN (1<<4) // 0x10
#define STATUS_UNKNOWN3  (1<<3) // 0x08
#define STATUS_UNKNOWN2  (1<<2) // 0x04
#define STATUS_ROTATING  (1<<1) // 0x02
#define STATUS_ERROR     (1<<0) // 0x01

/* Errors */
#define ERROR_NOTREADY   (1<<7) // 0x80
#define ERROR_INVALIDCMD (1<<6) // 0x40
#define ERROR_INVALIDARG (1<<5) // 0x20

// 1x = 75 sectors per second
// PSXCLK = 1 sec in the ps
// so (PSXCLK / 75) = cdr read time (linuzappz)
#define cdReadTime         (PSXCLK / 75) / 2  // OK
#define playAdpcmTime      (PSXCLK * 930 / 4 / 44100) / 2  // OK
#define WaitTime1st        (0x800)
#define WaitTime1stInit    (0x13cce >> 1)
#define WaitTime1stRead    (PSXCLK / 75)   // OK
#define WaitTime2ndGetID   (0x4a00)  // OK
#define WaitTime2ndPause   (cdReadTime * 3) // OK


enum drive_state {
	DRIVESTATE_STANDBY = 0,
	DRIVESTATE_LID_OPEN,
	DRIVESTATE_RESCAN_CD,
	DRIVESTATE_PREPARE_CD,
	DRIVESTATE_STOPPED,
};

// for cdr.Seeked
enum seeked_state {
	SEEK_PENDING = 0,
	SEEK_DONE = 1,
};

static struct CdrStat stat;

int msf2SectMNoItob[] = {
    0,4500,9000,13500,18000,22500,27000,31500,36000,40500,45000,49500,54000,58500,63000,67500,72000,76500,81000,85500,90000,94500,99000,103500,108000,112500,117000,
    121500,126000,130500,135000,139500,144000,148500,153000,157500,162000,166500,171000,175500,180000,184500,189000,193500,198000,202500,207000,211500,216000,220500,
    225000,229500,234000,238500,243000,247500,252000,256500,261000,265500,270000,274500,279000,283500,288000,292500,297000,301500,306000,310500,315000,319500,324000,
    328500,333000,337500,342000,346500,351000,355500,360000,364500,369000,373500,378000,382500,387000,391500,396000,400500,405000,409500,414000,418500,423000,427500,
    432000,436500,441000,445500,450000,454500,459000,463500,468000,472500,477000,481500,486000,490500,495000,499500,504000,508500,513000,517500,522000,526500,531000,
    535500,540000,544500,549000,553500,558000,562500,567000,571500,576000,580500,585000,589500,594000,598500,603000,607500,612000,616500,621000,625500,630000,634500,
    639000,643500,648000,652500,657000,661500,666000,670500,675000,679500,684000,688500,693000,697500,702000,706500,711000,715500,720000,724500,729000,733500,738000,
    742500,747000,751500,756000,760500,765000,769500,774000,778500,783000,787500,792000,796500,801000,805500,810000,814500,819000,823500,828000,832500,837000,841500,
    846000,850500,855000,859500,864000,868500,873000,877500,882000,886500,891000,895500,900000,904500,909000,913500,918000,922500,927000,931500,936000,940500,945000,
    949500,954000,958500,963000,967500,972000,976500,981000,985500,990000,994500,999000,1003500,1008000,1012500,1017000,1021500,1026000,1030500,1035000,1039500,1044000,
    1048500,1053000,1057500,1062000,1066500,1071000,1075500,1080000,1084500,1089000,1093500,1098000,1102500,1107000,1111500,1116000,1120500,1125000,1129500,1134000,1138500,1143000,1147500
};

int msf2SectSNoItob[] = {
    0,75,150,225,300,375,450,525,600,675,750,825,900,975,1050,1125,1200,1275,1350,1425,1500,1575,1650,1725,1800,1875,1950,2025,2100,2175,2250,2325,2400,2475,2550,2625,
    2700,2775,2850,2925,3000,3075,3150,3225,3300,3375,3450,3525,3600,3675,3750,3825,3900,3975,4050,4125,4200,4275,4350,4425,4500,4575,4650,4725,4800,4875,4950,5025,5100,
    5175,5250,5325,5400,5475,5550,5625,5700,5775,5850,5925,6000,6075,6150,6225,6300,6375,6450,6525,6600,6675,6750,6825,6900,6975,7050,7125,7200,7275,7350,7425,7500,7575,
    7650,7725,7800,7875,7950,8025,8100,8175,8250,8325,8400,8475,8550,8625,8700,8775,8850,8925,9000,9075,9150,9225,9300,9375,9450,9525,9600,9675,9750,9825,9900,9975,10050,
    10125,10200,10275,10350,10425,10500,10575,10650,10725,10800,10875,10950,11025,11100,11175,11250,11325,11400,11475,11550,11625,11700,11775,11850,11925,12000,12075,12150,
    12225,12300,12375,12450,12525,12600,12675,12750,12825,12900,12975,13050,13125,13200,13275,13350,13425,13500,13575,13650,13725,13800,13875,13950,14025,14100,14175,14250,
    14325,14400,14475,14550,14625,14700,14775,14850,14925,15000,15075,15150,15225,15300,15375,15450,15525,15600,15675,15750,15825,15900,15975,16050,16125,16200,16275,16350,
    16425,16500,16575,16650,16725,16800,16875,16950,17025,17100,17175,17250,17325,17400,17475,17550,17625,17700,17775,17850,17925,18000,18075,18150,18225,18300,18375,18450,
    18525,18600,18675,18750,18825,18900,18975,19050,19125
};

static unsigned int msf2sec(const u8 *msf) {
	//return ((msf[0] * 60 + msf[1]) * 75) + msf[2];
	return msf2SectMNoItob[msf[0]] + msf2SectSNoItob[msf[1]] + msf[2];
}

// for that weird psemu API..
static unsigned int fsm2sec(const u8 *msf) {
	//return ((msf[2] * 60 + msf[1]) * 75) + msf[0];
	return msf2SectMNoItob[msf[2]] + msf2SectSNoItob[msf[1]] + msf[0];
}

static void sec2msf(unsigned int s, u8 *msf) {
	msf[0] = s / 75 / 60;
	//s = s - msf[0] * 75 * 60;
	s = s - msf2SectMNoItob[msf[0]];
	msf[1] = s / 75;
	//s = s - msf[1] * 75;
	s = s - msf2SectSNoItob[msf[1]];
	msf[2] = s;
}

// cdrInterrupt
#define CDR_INT(eCycle) { \
	psxRegs.interrupt |= (1 << PSXINT_CDR); \
	psxRegs.intCycle[PSXINT_CDR].cycle = eCycle; \
	psxRegs.intCycle[PSXINT_CDR].sCycle = psxRegs.cycle; \
}

// cdrReadInterrupt
#define CDREAD_INT(eCycle) { \
	psxRegs.interrupt |= (1 << PSXINT_CDREAD); \
	psxRegs.intCycle[PSXINT_CDREAD].cycle = eCycle; \
	psxRegs.intCycle[PSXINT_CDREAD].sCycle = psxRegs.cycle; \
}

// cdrLidSeekInterrupt
#define CDRLID_INT(eCycle) { \
	psxRegs.interrupt |= (1 << PSXINT_CDRLID); \
	psxRegs.intCycle[PSXINT_CDRLID].cycle = eCycle; \
	psxRegs.intCycle[PSXINT_CDRLID].sCycle = psxRegs.cycle; \
}

// cdrPlayInterrupt
#define CDRMISC_INT(eCycle) { \
	psxRegs.interrupt |= (1 << PSXINT_CDRPLAY); \
	psxRegs.intCycle[PSXINT_CDRPLAY].cycle = eCycle; \
	psxRegs.intCycle[PSXINT_CDRPLAY].sCycle = psxRegs.cycle; \
}

#define StopReading() { \
	if (cdr.Reading) { \
		cdr.Reading = 0; \
		psxRegs.interrupt &= ~(1 << PSXINT_CDREAD); \
	} \
	cdr.StatP &= ~(STATUS_READ|STATUS_SEEK);\
}

#define StopCdda() { \
	if (cdr.Play) { \
		if (!Config.Cdda) CDR_stop(); \
		cdr.StatP &= ~STATUS_PLAY; \
		cdr.Play = FALSE; \
		cdr.FastForward = 0; \
		cdr.FastBackward = 0; \
		/*SPU_registerCallback( SPUirq );*/ \
	} \
}

#define SetResultSize(size) { \
	cdr.ResultP = 0; \
	cdr.ResultC = size; \
	cdr.ResultReady = 1; \
}

static void setIrq(void)
{
	if (cdr.Stat & cdr.Reg2) {
		psxHu32ref(0x1070) |= SWAP32((u32)0x4);
		psxRegs.interrupt|= 0x80000000;
	}
}

// timing used in this function was taken from tests on real hardware
// (yes it's slow, but you probably don't want to modify it)
void cdrLidSeekInterrupt()
{
	switch (cdr.DriveState) {
	default:
	    #ifdef DISP_DEBUG
        PRINT_LOG("cdrLidSeekInterrupt=default ");
        #endif // DISP_DEBUG
	case DRIVESTATE_STANDBY:
	    #ifdef DISP_DEBUG
        PRINT_LOG1("cdrLidSeekInterrupt=DRIVESTATE_STANDBY: %x ", stat.Status);
        #endif // DISP_DEBUG
		cdr.StatP &= ~STATUS_SEEK;

		if (CDR_getStatus(&stat) == -1)
			return;

        #ifdef DISP_DEBUG
        PRINT_LOG1("cdrLidSeekInterrupt=DRIVESTATE_STANDBY2: %x ", stat.Status);
        #endif // DISP_DEBUG
		if (stat.Status & STATUS_SHELLOPEN)
		{
			StopCdda();
			cdr.DriveState = DRIVESTATE_LID_OPEN;
			CDRLID_INT(WaitTime1st);
		}
		break;

	case DRIVESTATE_LID_OPEN:
	    #ifdef DISP_DEBUG
        PRINT_LOG1("cdrLidSeekInterrupt=DRIVESTATE_LID_OPEN: %x ", cdr.StatP);
        #endif // DISP_DEBUG
		if (CDR_getStatus(&stat) == -1)
			stat.Status &= ~STATUS_SHELLOPEN;

		// 02, 12, 10
		if (!(cdr.StatP & STATUS_SHELLOPEN)) {
			StopReading();
			cdr.StatP |= STATUS_SHELLOPEN;

			// could generate error irq here, but real hardware
			// only sometimes does that
			// (not done when lots of commands are sent?)

			CDRLID_INT(cdReadTime * 30);
			break;
		}
		else if (cdr.StatP & STATUS_ROTATING) {
			cdr.StatP &= ~STATUS_ROTATING;
		}
		else if (!(stat.Status & STATUS_SHELLOPEN)) {
			// closed now
			CheckCdrom();

			// cdr.StatP STATUS_SHELLOPEN is "sticky"
			// and is only cleared by CdlNop

			cdr.DriveState = DRIVESTATE_RESCAN_CD;
			CDRLID_INT(cdReadTime * 105);
			break;
		}

		// recheck for close
		CDRLID_INT(cdReadTime * 3);
		break;

	case DRIVESTATE_RESCAN_CD:
	    #ifdef DISP_DEBUG
        PRINT_LOG1("cdrLidSeekInterrupt=DRIVESTATE_RESCAN_CD: %x ", cdr.StatP);
        #endif // DISP_DEBUG
		cdr.StatP |= STATUS_ROTATING;
		cdr.DriveState = DRIVESTATE_PREPARE_CD;

		// this is very long on real hardware, over 6 seconds
		// make it a bit faster here...
		CDRLID_INT(cdReadTime * 150);
		break;

	case DRIVESTATE_PREPARE_CD:
	    #ifdef DISP_DEBUG
        PRINT_LOG1("cdrLidSeekInterrupt=DRIVESTATE_PREPARE_CD: %x ", cdr.StatP);
        #endif // DISP_DEBUG
		cdr.StatP |= STATUS_SEEK;

		cdr.DriveState = DRIVESTATE_STANDBY;
		CDRLID_INT(cdReadTime * 26);
		break;
	}
}

static void Find_CurTrack(const u8 *time)
{
	int current, sect;

	current = msf2sec(time);

	for (cdr.CurTrack = 1; cdr.CurTrack < cdr.ResultTN[1]; cdr.CurTrack++) {
		CDR_getTD(cdr.CurTrack + 1, cdr.ResultTD);
		sect = fsm2sec(cdr.ResultTD);
		if (sect - current >= 150)
			break;
	}
}

static void generate_subq(const u8 *time)
{
	unsigned char start[3], next[3];
	unsigned int this_s, start_s, next_s, pregap;
	int relative_s;

	CDR_getTD(cdr.CurTrack, start);
	if (cdr.CurTrack + 1 <= cdr.ResultTN[1]) {
		pregap = 150;
		CDR_getTD(cdr.CurTrack + 1, next);
	}
	else {
		// last track - cd size
		pregap = 0;
		next[0] = cdr.SetSectorEnd[2];
		next[1] = cdr.SetSectorEnd[1];
		next[2] = cdr.SetSectorEnd[0];
	}

	this_s = msf2sec(time);
	start_s = fsm2sec(start);
	next_s = fsm2sec(next);

	cdr.TrackChanged = FALSE;

	if (next_s - this_s < pregap) {
		cdr.TrackChanged = TRUE;
		cdr.CurTrack++;
		start_s = next_s;
	}

	cdr.subq.Index = 1;

	relative_s = this_s - start_s;
	if (relative_s < 0) {
		cdr.subq.Index = 0;
		relative_s = -relative_s;
	}
	sec2msf(relative_s, cdr.subq.Relative);

	cdr.subq.Track = itob(cdr.CurTrack);
	cdr.subq.Relative[0] = itob(cdr.subq.Relative[0]);
	cdr.subq.Relative[1] = itob(cdr.subq.Relative[1]);
	cdr.subq.Relative[2] = itob(cdr.subq.Relative[2]);
	cdr.subq.Absolute[0] = itob(time[0]);
	cdr.subq.Absolute[1] = itob(time[1]);
	cdr.subq.Absolute[2] = itob(time[2]);
}

static void ReadTrack(const u8 *time) {
	unsigned char tmp[3];
	struct SubQ *subq;
	u16 crc;

	tmp[0] = itob(time[0]);
	tmp[1] = itob(time[1]);
	tmp[2] = itob(time[2]);

	if (memcmp(cdr.Prev, tmp, 3) == 0)
		return;

	CDR_LOG("ReadTrack *** %02x:%02x:%02x\n", tmp[0], tmp[1], tmp[2]);

	cdr.NoErr = CDR_readTrack(tmp);
	memcpy(cdr.Prev, tmp, 3);

	//if (CheckSBI(time))
	//	return;

	subq = (struct SubQ *)CDR_getBufferSub();
	if (subq != NULL && cdr.CurTrack == 1) {
		crc = calcCrc((u8 *)subq + 12, 10);
		if (crc == (((u16)subq->CRC[0] << 8) | subq->CRC[1])) {
			cdr.subq.Track = subq->TrackNumber;
			cdr.subq.Index = subq->IndexNumber;
			memcpy(cdr.subq.Relative, subq->TrackRelativeAddress, 3);
			memcpy(cdr.subq.Absolute, subq->AbsoluteAddress, 3);
		}
		else {
			CDR_LOG_I("subq bad crc @%02x:%02x:%02x\n",
				tmp[0], tmp[1], tmp[2]);
		}
	}
	else {
		generate_subq(time);
	}

	CDR_LOG(" -> %02x,%02x %02x:%02x:%02x %02x:%02x:%02x\n",
		cdr.subq.Track, cdr.subq.Index,
		cdr.subq.Relative[0], cdr.subq.Relative[1], cdr.subq.Relative[2],
		cdr.subq.Absolute[0], cdr.subq.Absolute[1], cdr.subq.Absolute[2]);
}

static void AddIrqQueue(unsigned short irq, unsigned long ecycle) {
	if (cdr.Irq != 0) {
		if (irq == cdr.Irq || irq + 0x100 == cdr.Irq) {
			cdr.IrqRepeated = 1;
			CDR_INT(ecycle);
			return;
		}

		CDR_LOG_I("cdr: override cmd %02x -> %02x\n", cdr.Irq, irq);
	}

	cdr.Irq = irq;
	cdr.eCycle = ecycle;

	CDR_INT(ecycle);
}

static void cdrPlayDataEnd()
{
    #ifdef DISP_DEBUG
	sprintf(txtbuffer, "cdrPlayDataEnd cdr.Mode & MODE_AUTOPAUSE %d \n", cdr.Mode & MODE_AUTOPAUSE);
    DEBUG_print(txtbuffer, DBG_CDR1);
    #endif // DISP_DEBUG
	//if (cdr.Mode & MODE_AUTOPAUSE)
    {
		cdr.Stat = DataEnd;
        SetResultSize(1);
		cdr.StatP |= STATUS_ROTATING;
		cdr.StatP &= ~STATUS_SEEK;
		cdr.Result[0] = cdr.StatP;
		cdr.Seeked = SEEK_DONE;
		psxHu32ref(0x1070) |= SWAP32((u32)0x4);
		psxRegs.interrupt|= 0x80000000;

		StopCdda();
	}
}

static void cdrPlayInterrupt_Autopause()
{
	u32 abs_lev_max = 0;
	bool abs_lev_chselect;
	u32 i;

	if ((cdr.Mode & MODE_AUTOPAUSE) && cdr.TrackChanged) {
		//CDR_LOG( "CDDA STOP\n" );

		// Magic the Gathering
		// - looping territory cdda

		// ...?
		//cdr.ResultReady = 1;
		//cdr.Stat = DataReady;
		cdr.Stat = DataEnd;
		setIrq();

		StopCdda();
	}
	else if (((cdr.Mode & MODE_REPORT) || cdr.FastForward || cdr.FastBackward)) {
        #ifdef SHOW_DEBUG
        //DEBUG_print("Autopause CDR_readCDDA ===", DBG_CDR1);
        #endif // DISP_DEBUG
		//CDR_readCDDA(cdr.SetSectorPlay[0], cdr.SetSectorPlay[1], cdr.SetSectorPlay[2], (u8 *)read_buf);
		cdr.Result[0] = cdr.StatP;
		cdr.Result[1] = cdr.subq.Track;
		cdr.Result[2] = cdr.subq.Index;

		abs_lev_chselect = cdr.subq.Absolute[1] & 0x01;

		/* 8 is a hack. For accuracy, it should be 588. */
		for (i = 0; i < 8; i++)
		{
			abs_lev_max = MAX_VALUE(abs_lev_max, abs(read_buf[i * 2 + abs_lev_chselect]));
		}
		abs_lev_max = MIN_VALUE(abs_lev_max, 32767);
		abs_lev_max |= abs_lev_chselect << 15;

		if (cdr.subq.Absolute[2] & 0x10) {
			cdr.Result[3] = cdr.subq.Relative[0];
			cdr.Result[4] = cdr.subq.Relative[1] | 0x80;
			cdr.Result[5] = cdr.subq.Relative[2];
		}
		else {
			cdr.Result[3] = cdr.subq.Absolute[0];
			cdr.Result[4] = cdr.subq.Absolute[1];
			cdr.Result[5] = cdr.subq.Absolute[2];
		}

		cdr.Result[6] = abs_lev_max >> 0;
		cdr.Result[7] = abs_lev_max >> 8;

		// Rayman: Logo freeze (resultready + dataready)
		cdr.ResultReady = 1;
		cdr.Stat = DataReady;

		SetResultSize(8);
		setIrq();
	}
}

// called by playthread
static void cdrPlayCddaData(int timePlus, int isEnd)
{
	if (!cdr.Play) return;

	if (isEnd == 1) {
		StopCdda();
		cdr.TrackChanged = TRUE;
	}

	if (!cdr.Irq && !cdr.Stat && (cdr.Mode & (MODE_AUTOPAUSE | MODE_REPORT)))
		cdrPlayInterrupt_Autopause();

	cdr.SetSectorPlay[2] += timePlus;
	if (cdr.SetSectorPlay[2] >= 75) {
		cdr.SetSectorPlay[2] = 0;
		cdr.SetSectorPlay[1]++;
		if (cdr.SetSectorPlay[1] == 60) {
			cdr.SetSectorPlay[1] = 0;
			cdr.SetSectorPlay[0]++;
		}
	}

	// update for CdlGetlocP/autopause
	generate_subq(cdr.SetSectorPlay);
}

// also handles seek
void cdrPlayInterrupt()
{
	if (cdr.Seeked == SEEK_PENDING) {
		if (cdr.Stat) {
			CDR_LOG_I("cdrom: seek stat hack\n");
			CDRMISC_INT(0x1000);
			return;
		}
		SetResultSize(1);
		cdr.StatP |= STATUS_ROTATING;
		cdr.StatP &= ~STATUS_SEEK;
		cdr.Result[0] = cdr.StatP;
		cdr.Seeked = SEEK_DONE;
		if (cdr.Irq == 0) {
			cdr.Stat = Complete;
			setIrq();
		}

		if (cdr.SetlocPending) {
			//memcpy(cdr.SetSectorPlay, cdr.SetSector, 4);
			*((u32*)cdr.SetSectorPlay) = *((u32*)cdr.SetSector);
			cdr.SetlocPending = 0;
			cdr.m_locationChanged = TRUE;
		}
		Find_CurTrack(cdr.SetSectorPlay);
		ReadTrack(cdr.SetSectorPlay);
		cdr.TrackChanged = FALSE;
	}

	if (!cdr.Play) return;

	CDR_LOG( "CDDA - %d:%d:%d\n",
		cdr.SetSectorPlay[0], cdr.SetSectorPlay[1], cdr.SetSectorPlay[2] );

	if (memcmp(cdr.SetSectorPlay, cdr.SetSectorEnd, 3) == 0) {
		StopCdda();
		cdr.TrackChanged = TRUE;
	}
	//else {
	//	CDR_readCDDA(cdr.SetSectorPlay[0], cdr.SetSectorPlay[1], cdr.SetSectorPlay[2], (u8 *)read_buf);
	//}

	if (!cdr.Irq && !cdr.Stat && (cdr.Mode & (MODE_AUTOPAUSE|MODE_REPORT)))
		cdrPlayInterrupt_Autopause();

	//if (!cdr.Play) return;
	//#ifdef DISP_DEBUG
    //PRINT_LOG2("Bef CDR_readCDDA==Muted Mode %d %d", cdr.Muted, cdr.Mode);
    //#endif // DISP_DEBUG
	/*if (CDR_readCDDA && !cdr.Muted && !Config.Cdda) {
	//if (CDR_readCDDA && !cdr.Muted) {
        #ifdef SHOW_DEBUG
        sprintf(txtbuffer, "CDR_readCDDA time %d %d %d", cdr.SetSectorPlay[0], cdr.SetSectorPlay[1], cdr.SetSectorPlay[2]);
        DEBUG_print(txtbuffer, DBG_CDR2);
        #endif // DISP_DEBUG
		//CDR_readCDDA(cdr.SetSectorPlay[0], cdr.SetSectorPlay[1],
		//	cdr.SetSectorPlay[2], cdr.Transfer);

		//cdrAttenuate((s16 *)cdr.Transfer, CD_FRAMESIZE_RAW / 4, 1);
		//if (SPU_playCDDAchannel)
		//	SPU_playCDDAchannel((short *)cdr.Transfer, CD_FRAMESIZE_RAW);
		cdrAttenuate(read_buf, CD_FRAMESIZE_RAW / 4, 1);
		if (SPU_playCDDAchannel)
			SPU_playCDDAchannel(read_buf, CD_FRAMESIZE_RAW);
	}*/

	cdr.SetSectorPlay[2]++;
	if (cdr.SetSectorPlay[2] == 75) {
		cdr.SetSectorPlay[2] = 0;
		cdr.SetSectorPlay[1]++;
		if (cdr.SetSectorPlay[1] == 60) {
			cdr.SetSectorPlay[1] = 0;
			cdr.SetSectorPlay[0]++;
		}
	}

	if (cdr.m_locationChanged)
	{
		CDRMISC_INT(cdReadTime * 30);
		cdr.m_locationChanged = FALSE;
	}
	else
	{
		CDRMISC_INT(cdReadTime);
	}

	// update for CdlGetlocP/autopause
	generate_subq(cdr.SetSectorPlay);
}

void cdrInterrupt() {
	u16 Irq = cdr.Irq;
	int no_busy_error = 0;
	int start_rotating = 0;
	int error = 0;
	int delay;
	unsigned int seekTime = 0;
	u8 set_loc[3];
	int i;

	// Reschedule IRQ
	if (cdr.Stat) {
        #ifdef SHOW_DEBUG
        if (cdr.Irq > 0x100)
        {
            sprintf(txtbuffer, "cdrInterrupt1 (%s) cdr.Stat %x\n", CmdName[cdr.Irq - 0x100], cdr.Stat);
        }
        else
        {
            sprintf(txtbuffer, "cdrInterrupt1 (%s) cdr.Stat %x\n", CmdName[cdr.Irq], cdr.Stat);
        }
        DEBUG_print(txtbuffer, DBG_CDR2);
        writeLogFile(txtbuffer);
        #endif // DISP_DEBUG
		CDR_INT(WaitTime1st);
		return;
	}

	cdr.Ctrl &= ~0x80;

	// default response
	SetResultSize(1);
	cdr.Result[0] = cdr.StatP;
	cdr.Stat = Acknowledge;

	if (cdr.IrqRepeated) {
		cdr.IrqRepeated = 0;
		if (cdr.eCycle > psxRegs.cycle) {
			CDR_INT(cdr.eCycle);
			goto finish;
		}
	}
	#ifdef SHOW_DEBUG
	if (cdr.Irq > 0x100)
    {
        sprintf(txtbuffer, "cdrInterrupt2 (%s) cdr.Irq %02x cdr.Stat %x\n", CmdName[cdr.Irq - 0x100], cdr.Irq, cdr.Stat);
    }
    else
    {
        sprintf(txtbuffer, "cdrInterrupt2 (%s) cdr.Irq %02x cdr.Stat %x\n", CmdName[cdr.Irq], cdr.Irq, cdr.Stat);
    }

	DEBUG_print(txtbuffer, DBG_CDR2);
	writeLogFile(txtbuffer);
    #endif // DISP_DEBUG
	cdr.Irq = 0;

	switch (Irq) {
		case CdlNop:
			if (cdr.DriveState != DRIVESTATE_LID_OPEN)
				cdr.StatP &= ~STATUS_SHELLOPEN;
			no_busy_error = 1;
			break;

		case CdlSetloc:
			CDR_LOG("CDROM setloc command (%02X, %02X, %02X)\n", cdr.Param[0], cdr.Param[1], cdr.Param[2]);

			// MM must be BCD, SS must be BCD and <0x60, FF must be BCD and <0x75
			if (((cdr.Param[0] & 0x0F) > 0x09) || (cdr.Param[0] > 0x99) || ((cdr.Param[1] & 0x0F) > 0x09) || (cdr.Param[1] >= 0x60) || ((cdr.Param[2] & 0x0F) > 0x09) || (cdr.Param[2] >= 0x75))
			{
				CDR_LOG("Invalid/out of range seek to %02X:%02X:%02X\n", cdr.Param[0], cdr.Param[1], cdr.Param[2]);
				error = ERROR_INVALIDARG;
				goto set_error;
			}
			else
			{
				for (i = 0; i < 3; i++)
				{
					set_loc[i] = btoi(cdr.Param[i]);
				}

				i = msf2sec(cdr.SetSectorPlay);
				i = abs(i - msf2sec(set_loc));
				if (i > 16)
					cdr.Seeked = SEEK_PENDING;

				memcpy(cdr.SetSector, set_loc, 3);
				cdr.SetSector[3] = 0;
				cdr.SetlocPending = 1;
			}
			break;

		do_CdlPlay:
		case CdlPlay:
			StopCdda();
			if (cdr.Seeked == SEEK_PENDING) {
				// XXX: wrong, should seek instead..
				cdr.Seeked = SEEK_DONE;
			}

			cdr.FastBackward = 0;
			cdr.FastForward = 0;

			if (cdr.SetlocPending) {
				memcpy(cdr.SetSectorPlay, cdr.SetSector, 4);
				cdr.SetlocPending = 0;
				cdr.m_locationChanged = TRUE;
			}

			// BIOS CD Player
			// - Pause player, hit Track 01/02/../xx (Setloc issued!!)

			if (cdr.ParamC == 0 || cdr.Param[0] == 0) {
				CDR_LOG("PLAY Resume @ %d:%d:%d\n",
					cdr.SetSectorPlay[0], cdr.SetSectorPlay[1], cdr.SetSectorPlay[2]);
			}
			else
			{
				int track = btoi( cdr.Param[0] );

				if (track <= cdr.ResultTN[1])
					cdr.CurTrack = track;

				CDR_LOG("PLAY track %d\n", cdr.CurTrack);

				if (CDR_getTD((u8)cdr.CurTrack, cdr.ResultTD) != -1) {
					cdr.SetSectorPlay[0] = cdr.ResultTD[2];
					cdr.SetSectorPlay[1] = cdr.ResultTD[1];
					cdr.SetSectorPlay[2] = cdr.ResultTD[0];
				}
			}

			/*
			Rayman: detect track changes
			- fixes logo freeze

			Twisted Metal 2: skip PREGAP + starting accurate SubQ
			- plays tracks without retry play

			Wild 9: skip PREGAP + starting accurate SubQ
			- plays tracks without retry play
			*/
			Find_CurTrack(cdr.SetSectorPlay);
			ReadTrack(cdr.SetSectorPlay);
			cdr.TrackChanged = FALSE;

			StopReading();
			if (!Config.Cdda)
				CDR_play(cdr.SetSectorPlay);

			// Vib Ribbon: gameplay checks flag
			cdr.StatP &= ~STATUS_SEEK;
			cdr.Result[0] = cdr.StatP;

			cdr.StatP |= STATUS_PLAY;

			// BIOS player - set flag again
			cdr.Play = TRUE;

			CDRMISC_INT( cdReadTime );
			start_rotating = 1;
			break;

		case CdlForward:
			// TODO: error 80 if stopped
			cdr.Stat = Complete;
			// GameShark CD Player: Calls 2x + Play 2x
			cdr.FastForward = 1;
			cdr.FastBackward = 0;
			break;

		case CdlBackward:
			cdr.Stat = Complete;

			// GameShark CD Player: Calls 2x + Play 2x
			cdr.FastBackward =1;
			cdr.FastForward = 0;
			break;

		case CdlStandby:
			if (cdr.DriveState != DRIVESTATE_STOPPED) {
				error = ERROR_INVALIDARG;
				goto set_error;
			}
			//AddIrqQueue(CdlStandby + 0x100, cdReadTime * 125 / 2);
			AddIrqQueue(CdlStandby + 0x100, WaitTime1st);
			start_rotating = 1;
			break;

		case CdlStandby + 0x100:
			cdr.Stat = Complete;
			break;

		case CdlStop:
			if (cdr.Play) {
				// grab time for current track
				CDR_getTD((u8)(cdr.CurTrack), cdr.ResultTD);

				cdr.SetSectorPlay[0] = cdr.ResultTD[2];
				cdr.SetSectorPlay[1] = cdr.ResultTD[1];
				cdr.SetSectorPlay[2] = cdr.ResultTD[0];
			}

			StopCdda();
			StopReading();

			//delay = 0x800;
			//if (cdr.DriveState == DRIVESTATE_STANDBY)
			//	delay = cdReadTime * 30 / 2;

			cdr.DriveState = DRIVESTATE_STOPPED;
			//AddIrqQueue(CdlStop + 0x100, delay);
			cdr.StatP &= ~STATUS_ROTATING;
			cdr.Result[0] = cdr.StatP;
			cdr.Stat = Complete;
			break;

		case CdlStop + 0x100:
			cdr.StatP &= ~STATUS_ROTATING;
			cdr.Result[0] = cdr.StatP;
			cdr.Stat = Complete;
			break;

		case CdlPause:
			/*
			Gundam Battle Assault 2: much slower (*)
			- Fixes boot, gameplay
			Hokuto no Ken 2: slower
			- Fixes intro + subtitles
			InuYasha - Feudal Fairy Tale: slower
			- Fixes battles
			*/
			/* Gameblabla - Tightening the timings (as taken from Duckstation).
			 * The timings from Duckstation are based upon hardware tests.
			 * Mednafen's timing don't work for Gundam Battle Assault 2 in PAL/50hz mode,
			 * seems to be timing sensitive as it can depend on the CPU's clock speed.
			 *
			 * We will need to get around this for Bedlam/Rise 2 later...
			 * */
			/*if (cdr.DriveState == DRIVESTATE_STANDBY)
			{
				delay = 7000;
			}
			else
			{
				delay = (((cdr.Mode & MODE_SPEED) ? 2 : 1) * (1000000));
				CDRMISC_INT((cdr.Mode & MODE_SPEED) ? cdReadTime / 2 : cdReadTime);
			}
			AddIrqQueue(CdlPause + 0x100, delay);*/
			AddIrqQueue(CdlPause + 0x100, WaitTime2ndPause);
			cdr.Ctrl |= 0x80;
			break;

		case CdlPause + 0x100:
			cdr.StatP &= ~STATUS_READ;
			cdr.Result[0] = cdr.StatP;
			cdr.Stat = Complete;
			break;

		case CdlReset:
			cdr.Muted = FALSE;
			cdr.Mode = 0x20; /* Needed for This is Football 2, Pooh's Party and possibly others. */
			AddIrqQueue(CdlReset + 0x100, WaitTime1stInit);
			no_busy_error = 1;
			start_rotating = 1;
			break;

		case CdlReset + 0x100:
			cdr.Stat = Complete;
			break;

		case CdlMute:
			cdr.Muted = TRUE;
			break;

		case CdlDemute:
			cdr.Muted = FALSE;
			break;

		case CdlSetfilter:
			cdr.File = cdr.Param[0];
			cdr.Channel = cdr.Param[1];
			break;

		case CdlSetmode:
			no_busy_error = 1;
			break;

		case CdlGetparam:
			SetResultSize(5);
			cdr.Result[1] = cdr.Mode;
			cdr.Result[2] = 0;
			cdr.Result[3] = cdr.File;
			cdr.Result[4] = cdr.Channel;
			no_busy_error = 1;
			break;

		case CdlGetlocL:
			SetResultSize(8);
			memcpy(cdr.Result, cdr.Transfer, 8);
			break;

		case CdlGetlocP:
			SetResultSize(8);
			memcpy(&cdr.Result, &cdr.subq, 8);
			if (!cdr.Play && !cdr.Reading)
				cdr.Result[1] = 0; // HACK?
			break;

		case CdlReadT: // SetSession?
			// really long
			AddIrqQueue(CdlReadT + 0x100, cdReadTime * 290 / 4);
			start_rotating = 1;
			break;

		case CdlReadT + 0x100:
			cdr.Stat = Complete;
			break;

		case CdlGetTN:
			SetResultSize(3);
			if (CDR_getTN(cdr.ResultTN) == -1) {
				cdr.Stat = DiskError;
				cdr.Result[0] |= STATUS_ERROR;
			} else {
				cdr.Stat = Acknowledge;
				cdr.Result[1] = itob(cdr.ResultTN[0]);
				cdr.Result[2] = itob(cdr.ResultTN[1]);
			}
			break;

		case CdlGetTD:
			cdr.Track = btoi(cdr.Param[0]);
			SetResultSize(4);
			if (CDR_getTD(cdr.Track, cdr.ResultTD) == -1) {
				cdr.Stat = DiskError;
				cdr.Result[0] |= STATUS_ERROR;
			} else {
				cdr.Stat = Acknowledge;
				cdr.Result[0] = cdr.StatP;
				cdr.Result[1] = itob(cdr.ResultTD[2]);
				cdr.Result[2] = itob(cdr.ResultTD[1]);
				/* According to Nocash's documentation, the function doesn't care about ff.
				 * This can be seen also in Mednafen's implementation. */
				//cdr.Result[3] = itob(cdr.ResultTD[0]);
			}
			break;

		case CdlSeekL:
		case CdlSeekP:
			StopCdda();
			StopReading();
			cdr.StatP |= STATUS_SEEK;

			/*
			Crusaders of Might and Magic = 0.5x-4x
			- fix cutscene speech start

			Eggs of Steel = 2x-?
			- fix new game

			Medievil = ?-4x
			- fix cutscene speech

			Rockman X5 = 0.5-4x
			- fix capcom logo
			*/
			CDRMISC_INT(cdr.Seeked == SEEK_DONE ? 0x800 : cdReadTime * 4);
			cdr.Seeked = SEEK_PENDING;
			start_rotating = 1;
			break;

		case CdlTest:
			switch (cdr.Param[0]) {
				case 0x20: // System Controller ROM Version
					SetResultSize(4);
					memcpy(cdr.Result, Test20, 4);
					break;
				case 0x22:
					SetResultSize(8);
					memcpy(cdr.Result, Test22, 4);
					break;
				case 0x23: case 0x24:
					SetResultSize(8);
					memcpy(cdr.Result, Test23, 4);
					break;
			}
			no_busy_error = 1;
			break;

		case CdlID:
			AddIrqQueue(CdlID + 0x100, WaitTime2ndGetID);
			break;

		case CdlID + 0x100:
			SetResultSize(8);
			cdr.Result[0] = cdr.StatP;
			cdr.Result[1] = 0;
			cdr.Result[2] = 0;
			cdr.Result[3] = 0;

			// 0x10 - audio | 0x40 - disk missing | 0x80 - unlicensed
			if (CDR_getStatus(&stat) == -1 || stat.Type == 0 || stat.Type == 0xff) {
				cdr.Result[1] = 0xc0;
			}
			else {
				if (stat.Type == 2)
					cdr.Result[1] |= 0x10;
				if (CdromId[0] == '\0')
					cdr.Result[1] |= 0x80;
			}
			cdr.Result[0] |= (cdr.Result[1] >> 4) & 0x08;

			/* This adds the string "PCSX" in Playstation bios boot screen */
			//memcpy((char *)&cdr.Result[4], "PCSX", 4);
#ifdef HW_RVL
			strncpy((char *)&cdr.Result[4], "WSX ", 4);
#else
			strncpy((char *)&cdr.Result[4], "GCSX", 4);
#endif
			cdr.Stat = Complete;
			break;

		case CdlInit:
			// yes, it really sets STATUS_SHELLOPEN
			cdr.StatP |= STATUS_SHELLOPEN;
			cdr.DriveState = DRIVESTATE_RESCAN_CD;
			CDRLID_INT(20480);
			no_busy_error = 1;
			start_rotating = 1;
			break;

		case CdlGetQ:
			no_busy_error = 1;
			break;

		case CdlReadToc:
			AddIrqQueue(CdlReadToc + 0x100, cdReadTime * 180 / 4);
			no_busy_error = 1;
			start_rotating = 1;
			break;

		case CdlReadToc + 0x100:
			cdr.Stat = Complete;
			no_busy_error = 1;
			break;

		case CdlReadN:
		case CdlReadS:
			if (cdr.SetlocPending) {
				seekTime = abs(msf2sec(cdr.SetSectorPlay) - msf2sec(cdr.SetSector)) * (cdReadTime / 200);
				/*
				* Gameblabla :
				* It was originally set to 1000000 for Driver, however it is not high enough for Worms Pinball
				* and was unreliable for that game.
				* I also tested it against Mednafen and Driver's titlescreen music starts 25 frames later, not immediatly.
				*
				* Obviously, this isn't perfect but right now, it should be a bit better.
				* Games to test this against if you change that setting :
				* - Driver (titlescreen music delay and retry mission)
				* - Worms Pinball (Will either not boot or crash in the memory card screen)
				* - Viewpoint (short pauses if the delay in the ingame music is too long)
				*
				* It seems that 3386880 * 5 is too much for Driver's titlescreen and it starts skipping.
				* However, 1000000 is not enough for Worms Pinball to reliably boot.
				*/
				if(seekTime > 3386880 * 2) seekTime = 3386880 * 2;
				memcpy(cdr.SetSectorPlay, cdr.SetSector, 4);
				cdr.SetlocPending = 0;
				cdr.m_locationChanged = TRUE;
			}
			Find_CurTrack(cdr.SetSectorPlay);

        	#ifdef SHOW_DEBUG
            sprintf(txtbuffer, "READ_ACK Mode %d CurTrack %d \n", cdr.Mode & MODE_CDDA, cdr.CurTrack);
            DEBUG_print(txtbuffer, DBG_PROFILE_IDLE);
            writeLogFile(txtbuffer);
            #endif // DISP_DEBUG
			if ((cdr.Mode & MODE_CDDA) && cdr.CurTrack > 1)
				// Read* acts as play for cdda tracks in cdda mode
				goto do_CdlPlay;

			cdr.Reading = 1;
			cdr.FirstSector = 1;

			// Fighting Force 2 - update subq time immediately
			// - fixes new game
			ReadTrack(cdr.SetSectorPlay);


			// Crusaders of Might and Magic - update getlocl now
			// - fixes cutscene speech
			{
				u8 *buf = CDR_getBuffer();
				if (buf != NULL)
					memcpy(cdr.Transfer, buf, 8);
			}

			/*
			Duke Nukem: Land of the Babes - seek then delay read for one frame
			- fixes cutscenes
			C-12 - Final Resistance - doesn't like seek
			*/

			/*
				By nicolasnoble from PCSX Redux :
				"It LOOKS like this logic is wrong, therefore disabling it with `&& false` for now.
				For "PoPoLoCrois Monogatari II", the game logic will soft lock and will never issue GetLocP to detect
				the end of its XA streams, as it seems to assume ReadS will not return a status byte with the SEEK
				flag set. I think the reasonning is that since it's invalid to call GetLocP while seeking, the game
				tries to protect itself against errors by preventing from issuing a GetLocP while it knows the
				last status was "seek". But this makes the logic just softlock as it'll never get a notification
				about the fact the drive is done seeking and the read actually started.
				In other words, this state machine here is probably wrong in assuming the response to ReadS/ReadN is
				done right away. It's rather when it's done seeking, and the read has actually started. This probably
				requires a bit more work to make sure seek delays are processed properly.
				Checked with a few games, this seems to work fine."

				Gameblabla additional notes :
				This still needs the "+ seekTime" that PCSX Redux doesn't have for the Driver "retry" mission error.
			*/
			cdr.StatP |= STATUS_READ;
			cdr.StatP &= ~STATUS_SEEK;
			CDREAD_INT(((cdr.Mode & 0x80) ? (WaitTime1stRead) : WaitTime1stRead * 2) + seekTime);

			cdr.Result[0] = cdr.StatP;
			start_rotating = 1;
			break;

		case CdlSync:
		default:
			CDR_LOG_I("Invalid command: %02x\n", Irq);
			error = ERROR_INVALIDCMD;
			// FALLTHROUGH

		set_error:
			SetResultSize(2);
			cdr.Result[0] = cdr.StatP | STATUS_ERROR;
			cdr.Result[1] = error;
			cdr.Stat = DiskError;
			break;
	}

	if (cdr.DriveState == DRIVESTATE_STOPPED && start_rotating) {
		cdr.DriveState = DRIVESTATE_STANDBY;
		cdr.StatP |= STATUS_ROTATING;
	}

	if (!no_busy_error) {
		switch (cdr.DriveState) {
		case DRIVESTATE_LID_OPEN:
		case DRIVESTATE_RESCAN_CD:
		case DRIVESTATE_PREPARE_CD:
			SetResultSize(2);
			cdr.Result[0] = cdr.StatP | STATUS_ERROR;
			cdr.Result[1] = ERROR_NOTREADY;
			cdr.Stat = DiskError;
			break;
		}
	}

finish:
	setIrq();
	cdr.ParamC = 0;

#ifdef CDR_LOG_CMD_IRQ
	{
		int i;
		SysPrintf("CDR IRQ %d cmd %02x stat %02x: ",
			!!(cdr.Stat & cdr.Reg2), Irq, cdr.Stat);
		for (i = 0; i < cdr.ResultC; i++)
			SysPrintf("%02x ", cdr.Result[i]);
		SysPrintf("\n");
	}
#endif
}

#define ssat32_to_16(v) { \
  if (v < -32768) v = -32768; \
  else if (v > 32767) v = 32767; \
}

void cdrAttenuate(s16 *buf, int samples, int stereo)
{
	int i, l, r;
	int ll = cdr.AttenuatorLeftToLeft;
	int lr = cdr.AttenuatorLeftToRight;
	int rl = cdr.AttenuatorRightToLeft;
	int rr = cdr.AttenuatorRightToRight;

	if (lr == 0 && rl == 0 && 0x78 <= ll && ll <= 0x88 && 0x78 <= rr && rr <= 0x88)
		return;

	if (!stereo && ll == 0x40 && lr == 0x40 && rl == 0x40 && rr == 0x40)
		return;

	if (stereo) {
		for (i = 0; i < samples; i++) {
			l = buf[i * 2];
			r = buf[i * 2 + 1];
			l = (l * ll + r * rl) >> 7;
			r = (r * rr + l * lr) >> 7;
			ssat32_to_16(l);
			ssat32_to_16(r);
			buf[i * 2] = l;
			buf[i * 2 + 1] = r;
		}
	}
	else {
		for (i = 0; i < samples; i++) {
			l = buf[i];
			l = l * (ll + rl) >> 7;
			//r = r * (rr + lr) >> 7;
			ssat32_to_16(l);
			//ssat32_to_16(r);
			buf[i] = l;
		}
	}
}

void cdrReadInterrupt() {
	u8 *buf;

	if (!cdr.Reading)
    {
        return;
    }

	#ifdef SHOW_DEBUG
	sprintf(txtbuffer, "ReadInterrupt (%s) %x cdr.NoErr %d Channel %d \n", CmdName[cdr.Irq], cdr.Stat, cdr.NoErr, cdr.Channel);
	DEBUG_print(txtbuffer, DBG_CDR3);
	writeLogFile(txtbuffer);
    #endif // DISP_DEBUG
	if (cdr.Irq || cdr.Stat) {
		CDR_LOG_I("cdrom: read stat hack %02x %x\n", cdr.Irq, cdr.Stat);
		CDREAD_INT(0x100);
		return;
	}

	if ((psxHu32ref(0x1070) & psxHu32ref(0x1074) & SWAP32((u32)0x4)) && !cdr.ReadRescheduled) {
		// HACK: with BIAS 2, emulated CPU is often slower than real thing,
		// game may be unfinished with prev data read, so reschedule
		// (Brave Fencer Musashi)
		CDREAD_INT(cdReadTime / 2);
		cdr.ReadRescheduled = 1;
		return;
	}

	cdr.OCUP = 1;
	SetResultSize(1);
	cdr.StatP |= STATUS_READ|STATUS_ROTATING;
	cdr.StatP &= ~STATUS_SEEK;
	cdr.Result[0] = cdr.StatP;
	cdr.Seeked = SEEK_DONE;

	ReadTrack(cdr.SetSectorPlay);

	buf = CDR_getBuffer();
	if (buf == NULL)
		cdr.NoErr = 0;

	if (cdr.NoErr == 0) {
        #ifdef SHOW_DEBUG
        sprintf(txtbuffer, "ReadInterrupt cdr.RErr \n");
        DEBUG_print(txtbuffer, DBG_CDR4);
        writeLogFile(txtbuffer);
        #endif // DISP_DEBUG
		CDR_LOG_I("cdrReadInterrupt() Log: err\n");
		memset(cdr.Transfer, 0, DATA_SIZE);
		cdr.Stat = DiskError;
		cdr.Result[0] |= STATUS_ERROR;
		CDREAD_INT((cdr.Mode & 0x80) ? (cdReadTime / 2) : cdReadTime);
		return;
	}

	memcpy(cdr.Transfer, buf, DATA_SIZE);
	//CheckPPFCache(cdr.Transfer, cdr.Prev[0], cdr.Prev[1], cdr.Prev[2]);

#ifdef CDR_LOG
	//fprintf(emuLog, "cdrReadInterrupt() Log: cdr.Transfer %x:%x:%x\n", cdr.Transfer[0], cdr.Transfer[1], cdr.Transfer[2]);
#endif

	if ((!cdr.Muted) && (cdr.Mode & MODE_STRSND) && (!Config.Xa) && (cdr.FirstSector != -1)) { // CD-XA
		// Firemen 2: Multi-XA files - briefings, cutscenes
		if( cdr.FirstSector == 1 && (cdr.Mode & MODE_SF)==0 ) {
			cdr.File = cdr.Transfer[4 + 0];
			cdr.Channel = cdr.Transfer[4 + 1];
		}

		/* Gameblabla
		 * Skips playing on channel 255.
		 * Fixes missing audio in Blue's Clues : Blue's Big Musical. (Should also fix Taxi 2)
		 * TODO : Check if this is the proper behaviour.
		 * */
		if((cdr.Transfer[4 + 2] & 0x4) &&
			 (cdr.Transfer[4 + 1] == cdr.Channel) &&
			 (cdr.Transfer[4 + 0] == cdr.File) && cdr.Channel != 255) {

            cdr.PlayAdpcm = TRUE;
            //LWP_CreateThread(&threadPlayAdpcm, playAdpcmThread, NULL, NULL, 0, 80);
            //return;

			int ret = xa_decode_sector(&cdr.Xa, cdr.Transfer+4, cdr.FirstSector);
			#ifdef SHOW_DEBUG
            sprintf(txtbuffer, "playADPCMchannel ret %d\n", ret);
            DEBUG_print(txtbuffer, DBG_CDR4);
            writeLogFile(txtbuffer);
            #endif // DISP_DEBUG
			if (!ret) {
				cdrAttenuate(cdr.Xa.pcm, cdr.Xa.nsamples, cdr.Xa.stereo);
				/*
				 * Gameblabla -
				 * This is a hack for Megaman X4, Castlevania etc...
				 * that regressed from the new m_locationChanged and CDROM timings changes.
				 * It is mostly noticeable in Castevania however and the stuttering can be very jarring.
				 *
				 * According to PCSX redux authors, we shouldn't cause a location change if
				 * the sector difference is too small.
				 * I attempted to go with that approach but came empty handed.
				 * So for now, let's just set cdr.m_locationChanged to false when playing back any ADPCM samples.
				 * This does not regress Crash Team Racing's intro at least.
				*/
				cdr.m_locationChanged = FALSE;
				SPU_playADPCMchannel(&cdr.Xa);
				cdr.FirstSector = 0;
			}
			else cdr.FirstSector = -1;
		}
	}

	cdr.SetSectorPlay[2]++;
	if (cdr.SetSectorPlay[2] == 75) {
		cdr.SetSectorPlay[2] = 0;
		cdr.SetSectorPlay[1]++;
		if (cdr.SetSectorPlay[1] == 60) {
			cdr.SetSectorPlay[1] = 0;
			cdr.SetSectorPlay[0]++;
		}
	}

	cdr.Readed = 0;
	cdr.ReadRescheduled = 0;

	uint32_t delay = (cdr.Mode & MODE_SPEED) ? (cdReadTime / 2) : cdReadTime;
	if (cdr.m_locationChanged) {
		CDREAD_INT(delay * 30);
		cdr.m_locationChanged = FALSE;
	} else {
	    CDREAD_INT(delay);
	    /*if (cdr.PlayAdpcm)
        {
            CDREAD_INT((cdr.Mode & 0x80) ? (playAdpcmTime / 2) : playAdpcmTime);
		}
		else
		{
		    CDREAD_INT(delay);
		}*/
	}

	/*
	Croc 2: $40 - only FORM1 (*)
	Judge Dredd: $C8 - only FORM1 (*)
	Sim Theme Park - no adpcm at all (zero)
	*/

	if (!(cdr.Mode & MODE_STRSND) || !(cdr.Transfer[4+2] & 0x4)) {
		cdr.Stat = DataReady;
		setIrq();
	}

	// update for CdlGetlocP
	ReadTrack(cdr.SetSectorPlay);
}

/*
cdrRead0:
	bit 0,1 - mode
	bit 2 - unknown
	bit 3 - unknown
	bit 4 - unknown
	bit 5 - 1 result ready
	bit 6 - 1 dma ready
	bit 7 - 1 command being processed
*/

unsigned char cdrRead0(void) {
	if (cdr.ResultReady)
		cdr.Ctrl |= 0x20;
	else
		cdr.Ctrl &= ~0x20;

	if (cdr.OCUP)
		cdr.Ctrl |= 0x40;
//  else
//		cdr.Ctrl &= ~0x40;

	// What means the 0x10 and the 0x08 bits? I only saw it used by the bios
	cdr.Ctrl |= 0x18;

	CDR_LOG_IO("cdr r0: %02x\n", cdr.Ctrl);

	return psxHu8(0x1800) = cdr.Ctrl;
}

void cdrWrite0(unsigned char rt) {
	CDR_LOG_IO("cdr w0: %02x\n", rt);

	cdr.Ctrl = (rt & 3) | (cdr.Ctrl & ~3);
}

unsigned char cdrRead1(void) {
	if ((cdr.ResultP & 0xf) < cdr.ResultC)
		psxHu8(0x1801) = cdr.Result[cdr.ResultP & 0xf];
	else
		psxHu8(0x1801) = 0;
	cdr.ResultP++;
	if (cdr.ResultP == cdr.ResultC)
		cdr.ResultReady = 0;

	CDR_LOG_IO("cdr r1: %02x\n", psxHu8(0x1801));

	return psxHu8(0x1801);
}

void cdrWrite1(unsigned char rt) {
	CDR_LOG_IO("cdr w1: %02x\n", rt);

	switch (cdr.Ctrl & 3) {
	case 0:
		break;
	case 3:
		cdr.AttenuatorRightToRightT = rt;
		return;
	default:
		return;
	}

	cdr.Cmd = rt;
	cdr.OCUP = 0;

#ifdef CDR_LOG_CMD_IRQ
	SysPrintf("CD1 write: %x (%s)", rt, CmdName[rt]);
	if (cdr.ParamC) {
		SysPrintf(" Param[%d] = {", cdr.ParamC);
		for (i = 0; i < cdr.ParamC; i++)
			SysPrintf(" %x,", cdr.Param[i]);
		SysPrintf("}\n");
	} else {
		SysPrintf("\n");
	}
#endif

	cdr.ResultReady = 0;
	cdr.Ctrl |= 0x80;
	// cdr.Stat = NoIntr;
	AddIrqQueue(cdr.Cmd, WaitTime1st);

	switch (cdr.Cmd) {
	case CdlReadN:
	case CdlReadS:
	case CdlPause:
		StopCdda();
		StopReading();
		break;

	case CdlInit:
	case CdlReset:
		cdr.Seeked = SEEK_DONE;
		StopCdda();
		StopReading();
		break;

    	case CdlSetmode:
		CDR_LOG("cdrWrite1() Log: Setmode %x\n", cdr.Param[0]);

        	cdr.Mode = cdr.Param[0];

		// Squaresoft on PlayStation 1998 Collector's CD Vol. 1
		// - fixes choppy movie sound
		if( cdr.Play && (cdr.Mode & MODE_CDDA) == 0 )
			StopCdda();
        	break;
	}
}

unsigned char cdrRead2(void) {
	unsigned char ret;

	if (cdr.Readed == 0) {
		ret = 0;
	} else {
		ret = *pTransfer++;
	}

	CDR_LOG_IO("cdr r2: %02x\n", ret);
	return ret;
}

void cdrWrite2(unsigned char rt) {
	CDR_LOG_IO("cdr w2: %02x\n", rt);

	switch (cdr.Ctrl & 3) {
	case 0:
		if (cdr.ParamC < 8) // FIXME: size and wrapping
			cdr.Param[cdr.ParamC++] = rt;
		return;
	case 1:
		cdr.Reg2 = rt;
		setIrq();
		return;
	case 2:
		cdr.AttenuatorLeftToLeftT = rt;
		return;
	case 3:
		cdr.AttenuatorRightToLeftT = rt;
		return;
	}
}

unsigned char cdrRead3(void) {
	if (cdr.Ctrl & 0x1)
		psxHu8(0x1803) = cdr.Stat | 0xE0;
	else
		psxHu8(0x1803) = cdr.Reg2 | 0xE0;

	CDR_LOG_IO("cdr r3: %02x\n", psxHu8(0x1803));
	return psxHu8(0x1803);
}

void cdrWrite3(unsigned char rt) {
	CDR_LOG_IO("cdr w3: %02x\n", rt);

	switch (cdr.Ctrl & 3) {
	case 0:
		break; // transfer
	case 1:
		cdr.Stat &= ~rt;

		if (rt & 0x40)
			cdr.ParamC = 0;
		return;
	case 2:
		cdr.AttenuatorLeftToRightT = rt;
		return;
	case 3:
		if (rt & 0x20) {
			memcpy(&cdr.AttenuatorLeftToLeft, &cdr.AttenuatorLeftToLeftT, 4);
			CDR_LOG_I("CD-XA Volume: %02x %02x | %02x %02x\n",
				cdr.AttenuatorLeftToLeft, cdr.AttenuatorLeftToRight,
				cdr.AttenuatorRightToLeft, cdr.AttenuatorRightToRight);
		}
		return;
	}

	if ((rt & 0x80) && cdr.Readed == 0) {
		cdr.Readed = 1;
		pTransfer = cdr.Transfer;

		switch (cdr.Mode & 0x30) {
			case MODE_SIZE_2328:
			case 0x00:
				pTransfer += 12;
				break;

			case MODE_SIZE_2340:
				pTransfer += 0;
				break;

			default:
				break;
		}
	}
}

void psxDma3(u32 madr, u32 bcr, u32 chcr) {
	u32 cdsize;
	int size;
	u8 *ptr;

	CDR_LOG("psxDma3() Log: *** DMA 3 *** %x addr = %x size = %x\n", chcr, madr, bcr);

	switch (chcr) {
		case 0x11000000:
		case 0x11400100:
			if (cdr.Readed == 0) {
				CDR_LOG("psxDma3() Log: *** DMA 3 *** NOT READY\n");
				break;
			}

			cdsize = (bcr & 0xffff) * 4;

			// Ape Escape: bcr = 0001 / 0000
			// - fix boot
			if( cdsize == 0 )
			{
				switch (cdr.Mode & (MODE_SIZE_2340|MODE_SIZE_2328)) {
					case MODE_SIZE_2340: cdsize = 2340; break;
					case MODE_SIZE_2328: cdsize = 2328; break;
					default:
					case MODE_SIZE_2048: cdsize = 2048; break;
				}
			}


			ptr = (u8 *)PSXM(madr);
			if (ptr == NULL) {
				CDR_LOG("psxDma3() Log: *** DMA 3 *** NULL Pointer!\n");
				break;
			}

			/*
			GS CDX: Enhancement CD crash
			- Setloc 0:0:0
			- CdlPlay
			- Spams DMA3 and gets buffer overrun
			*/
			size = CD_FRAMESIZE_RAW - (pTransfer - cdr.Transfer);
			if (size > cdsize)
				size = cdsize;
			if (size > 0)
			{
				memcpy(ptr, pTransfer, size);
			}

			psxCpu->Clear(madr, cdsize / 4);
			pTransfer += cdsize;

			if( chcr == 0x11400100 ) {
				HW_DMA3_MADR = SWAPu32(madr + cdsize);
				CDRDMA_INT( (cdsize/4) / 4 );
			}
			else if( chcr == 0x11000000 ) {
				// CDRDMA_INT( (cdsize/4) * 1 );
				// halted
				psxRegs.cycle += (cdsize/4) * 24/2;
				CDRDMA_INT(16);
			}
			return;

		default:
#ifdef CDR_LOG
			CDR_LOG("psxDma3() Log: Unknown cddma %lx\n", chcr);
#endif
			break;
	}

	HW_DMA3_CHCR &= SWAP32(~0x01000000);
	DMA_INTERRUPT(3);
}

void cdrDmaInterrupt()
{
	if (HW_DMA3_CHCR & SWAP32(0x01000000))
	{
		HW_DMA3_CHCR &= SWAP32(~0x01000000);
		DMA_INTERRUPT(3);
	}
}

static void getCdInfo(void)
{
	u8 tmp;

	CDR_getTN(cdr.ResultTN);
	CDR_getTD(0, cdr.SetSectorEnd);
	tmp = cdr.SetSectorEnd[0];
	cdr.SetSectorEnd[0] = cdr.SetSectorEnd[2];
	cdr.SetSectorEnd[2] = tmp;
}

void cdrReset() {
	memset(&cdr, 0, sizeof(cdr));
	cdr.CurTrack = 1;
	cdr.File = 1;
	cdr.Channel = 1;
	cdr.Reg2 = 0x1f;
	cdr.Stat = NoIntr;
	cdr.DriveState = DRIVESTATE_STANDBY;
	cdr.StatP = STATUS_ROTATING;
	pTransfer = cdr.Transfer;
	cdr.SetlocPending = 0;
	cdr.m_locationChanged = FALSE;

	// BIOS player - default values
	cdr.AttenuatorLeftToLeft = 0x80;
	cdr.AttenuatorLeftToRight = 0x00;
	cdr.AttenuatorRightToLeft = 0x00;
	cdr.AttenuatorRightToRight = 0x80;
	getCdInfo();

	p_cdrPlayDataEnd = cdrPlayDataEnd;
	p_cdrPlayCddaData = cdrPlayCddaData;
}

int cdrFreeze(gzFile f, int Mode) {
	u32 tmp;
	u8 tmpp[3];

	if (Mode == 0 && !Config.Cdda)
		CDR_stop();

	cdr.freeze_ver = 0x63647202;
	gzfreeze(&cdr, sizeof(cdr));

	if (Mode == 1) {
		cdr.ParamP = cdr.ParamC;
		tmp = pTransfer - cdr.Transfer;
	}

	gzfreeze(&tmp, sizeof(tmp));

	if (Mode == 0) {
		getCdInfo();

		pTransfer = cdr.Transfer + tmp;

		// read right sub data
		tmpp[0] = btoi(cdr.Prev[0]);
		tmpp[1] = btoi(cdr.Prev[1]);
		tmpp[2] = btoi(cdr.Prev[2]);
		cdr.Prev[0]++;
		ReadTrack(tmpp);

		if (cdr.Play) {
			if (cdr.freeze_ver < 0x63647202)
				memcpy(cdr.SetSectorPlay, cdr.SetSector, 3);

			Find_CurTrack(cdr.SetSectorPlay);
			if (!Config.Cdda)
				CDR_play(cdr.SetSectorPlay);
		}

		if ((cdr.freeze_ver & 0xffffff00) != 0x63647200) {
			// old versions did not latch Reg2, have to fixup..
			if (cdr.Reg2 == 0) {
				SysPrintf("cdrom: fixing up old savestate\n");
				cdr.Reg2 = 7;
			}
			// also did not save Attenuator..
			if ((cdr.AttenuatorLeftToLeft | cdr.AttenuatorLeftToRight
			     | cdr.AttenuatorRightToLeft | cdr.AttenuatorRightToRight) == 0)
			{
				cdr.AttenuatorLeftToLeft = cdr.AttenuatorRightToRight = 0x80;
			}
		}
	}

	return 0;
}

void LidInterrupt() {
	getCdInfo();
	StopCdda();

    cdr.StatP |= STATUS_SHELLOPEN;
    cdr.DriveState = DRIVESTATE_RESCAN_CD;

	cdrLidSeekInterrupt();
}
