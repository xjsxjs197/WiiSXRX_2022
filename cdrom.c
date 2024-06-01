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

#include <assert.h>
#include "cdrom.h"
#include "misc.h"
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

static struct {
	// unused members maintain savesate compatibility
	unsigned char unused0;
	unsigned char unused1;
	unsigned char IrqMask;
	unsigned char unused2;
	unsigned char Ctrl;
	unsigned char IrqStat;

	unsigned char StatP;

	unsigned char Transfer[DATA_SIZE];
	struct {
		unsigned char Track;
		unsigned char Index;
		unsigned char Relative[3];
		unsigned char Absolute[3];
	} subq;
	unsigned char TrackChanged;
	unsigned char ReportDelay;
	unsigned char unused3;
	unsigned short sectorsRead;
	unsigned int  freeze_ver;

	unsigned char Prev[4];
	unsigned char Param[8];
	unsigned char Result[16];

	unsigned char ParamC;
	unsigned char ParamP;
	unsigned char ResultC;
	unsigned char ResultP;
	unsigned char ResultReady;
	unsigned char Cmd;
	unsigned char SubqForwardSectors;
	unsigned char SetlocPending;
	u32 Reading;

	unsigned char ResultTN[6];
	unsigned char ResultTD[4];
	unsigned char SetSectorPlay[4];
	unsigned char SetSectorEnd[4];
	unsigned char SetSector[4];
	unsigned char Track;
	bool Play, Muted;
	int CurTrack;
	unsigned char Mode;
	unsigned char FileChannelSelected;
	unsigned char CurFile, CurChannel;
	int FilterFile, FilterChannel;
	unsigned char LocL[8];
	int unused4;

	xa_decode_t Xa;

	u16 FifoOffset;
	u16 FifoSize;

	u16 CmdInProgress;
	u8 Irq1Pending;
	u8 AdpcmActive;
	u32 LastReadSeekCycles;

	u8 unused7;

	u8 DriveState;
	u8 FastForward;
	u8 FastBackward;
	u8 errorRetryhack;

	u8 AttenuatorLeftToLeft, AttenuatorLeftToRight;
	u8 AttenuatorRightToRight, AttenuatorRightToLeft;
	u8 AttenuatorLeftToLeftT, AttenuatorLeftToRightT;
	u8 AttenuatorRightToRightT, AttenuatorRightToLeftT;

	unsigned char SetSectorPlayBak[4];
} cdr;

//cdrStruct cdr;
//static unsigned char *pTransfer;
static s16 read_buf[CD_FRAMESIZE_RAW / 2];
static bool isShellopen;
extern char fastLoad; // variable for see if game has reduce load time

/* CD-ROM magic numbers */
#define CdlSync        0  /* nocash documentation : "Uh, actually, returns error code 40h = Invalid Command...?" */
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

// cdr.IrqStat:
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
#define STATUS_SEEKERROR (1<<2) // 0x04
#define STATUS_ROTATING  (1<<1) // 0x02
#define STATUS_ERROR     (1<<0) // 0x01

/* Errors */
#define ERROR_NOTREADY   (1<<7) // 0x80
#define ERROR_INVALIDCMD (1<<6) // 0x40
#define ERROR_BAD_ARGNUM (1<<5) // 0x20
#define ERROR_BAD_ARGVAL (1<<4) // 0x10
#define ERROR_SHELLOPEN  (1<<3) // 0x08

// 1x = 75 sectors per second
// PSXCLK = 1 sec in the ps
// so (PSXCLK / 75) = cdr read time (linuzappz)
#define cdReadTime         (PSXCLK / 75)  // OK
#define WaitTime1st        (0x800)

#define MinSeekTime        50000

#define LOCL_INVALID 0xff
#define SUBQ_FORWARD_SECTORS 2u

enum drive_state {
	DRIVESTATE_STANDBY = 0, // different from paused
	DRIVESTATE_LID_OPEN,
	DRIVESTATE_RESCAN_CD,
	DRIVESTATE_PREPARE_CD,
	DRIVESTATE_STOPPED,
	DRIVESTATE_PAUSED,
	DRIVESTATE_PLAY_READ,
	DRIVESTATE_SEEK,
};

static struct CdrStat cdr_stat;

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

// cdrPlayReadInterrupt
#define CDRPLAYREAD_INT(eCycle, isFirst) { \
	u32 e_ = eCycle; \
	psxRegs.interrupt |= (1 << PSXINT_CDREAD); \
	if (isFirst) \
		psxRegs.intCycle[PSXINT_CDREAD].sCycle = psxRegs.cycle; \
	else \
		psxRegs.intCycle[PSXINT_CDREAD].sCycle += psxRegs.intCycle[PSXINT_CDREAD].cycle; \
	psxRegs.intCycle[PSXINT_CDREAD].cycle = e_; \
	set_event_raw_abs(PSXINT_CDREAD, psxRegs.intCycle[PSXINT_CDREAD].sCycle + e_); \
}

#define StopReading() { \
	cdr.Reading = 0; \
	psxRegs.interrupt &= ~(1 << PSXINT_CDREAD); \
}

#define StopCdda() { \
	if (cdr.Play && !Config.Cdda) CDR_stop(); \
	cdr.Play = FALSE; \
	cdr.FastForward = 0; \
	cdr.FastBackward = 0; \
}

#define SetPlaySeekRead(x, f) { \
	x &= ~(STATUS_PLAY | STATUS_SEEK | STATUS_READ); \
	x |= f; \
}

#define SetResultSize_(size) { \
	cdr.ResultP = 0; \
	cdr.ResultC = size; \
	cdr.ResultReady = 1; \
}

#define SetResultSize(size) { \
	if (cdr.ResultP < cdr.ResultC) \
		CDR_LOG_I("overwriting result, len=%u\n", cdr.ResultC); \
	SetResultSize_(size); \
}

static void setIrq(u8 irq, int log_cmd)
{
	u8 old = cdr.IrqStat & cdr.IrqMask ? 1 : 0;
	u8 new_ = irq & cdr.IrqMask ? 1 : 0;

	cdr.IrqStat = irq;
	if ((old ^ new_) & new_)
		psxHu32ref(0x1070) |= SWAP32((u32)0x4);
}

// timing used in this function was taken from tests on real hardware
// (yes it's slow, but you probably don't want to modify it)
void cdrLidSeekInterrupt(void)
{
	switch (cdr.DriveState) {
	default:
	    #ifdef DISP_DEBUG
        sprintf(txtbuffer, "cdrLidSeekInterrupt=default ");
        DEBUG_print(txtbuffer, DBG_CDR4);
        #endif // DISP_DEBUG
	case DRIVESTATE_STANDBY:
	    #ifdef DISP_DEBUG
        sprintf(txtbuffer, "cdrLidSeekInterrupt=DRIVESTATE_STANDBY: %x ", cdr_stat.Status);
        DEBUG_print(txtbuffer, DBG_CDR4);
        #endif // DISP_DEBUG
		StopCdda();
		//StopReading();
		SetPlaySeekRead(cdr.StatP, 0);

		if (CDR_getStatus(&cdr_stat) == -1)
			return;

        #ifdef DISP_DEBUG
        sprintf(txtbuffer, "cdrLidSeekInterrupt=DRIVESTATE_STANDBY2: %x %d ", cdr_stat.Status, isShellopen);
        DEBUG_print(txtbuffer, DBG_CDR4);
        #endif // DISP_DEBUG
		if (cdr_stat.Status & STATUS_SHELLOPEN)
		{
			//isShellopen = false;
			memset(cdr.Prev, 0xff, sizeof(cdr.Prev));
			cdr.DriveState = DRIVESTATE_LID_OPEN;
			set_event(PSXINT_CDRLID, WaitTime1st);
		}
		break;

	case DRIVESTATE_LID_OPEN:
	    #ifdef DISP_DEBUG
        sprintf(txtbuffer, "cdrLidSeekInterrupt=DRIVESTATE_LID_OPEN: %x ", cdr.StatP);
        DEBUG_print(txtbuffer, DBG_CDR4);
        #endif // DISP_DEBUG
		if (CDR_getStatus(&cdr_stat) == -1)
			cdr_stat.Status &= ~STATUS_SHELLOPEN;

		// 02, 12, 10
		if (!(cdr.StatP & STATUS_SHELLOPEN)) {
			StopReading();
			SetPlaySeekRead(cdr.StatP, 0);
			cdr.StatP |= STATUS_SHELLOPEN;

			// IIRC this sometimes doesn't happen on real hw
			// (when lots of commands are sent?)
			SetResultSize(2);
			cdr.Result[0] = cdr.StatP | STATUS_SEEKERROR;
			cdr.Result[1] = ERROR_SHELLOPEN;
			if (cdr.CmdInProgress) {
				psxRegs.interrupt &= ~(1 << PSXINT_CDR);
				cdr.CmdInProgress = 0;
				cdr.Result[0] = cdr.StatP | STATUS_ERROR;
				cdr.Result[1] = ERROR_NOTREADY;
			}
			setIrq(DiskError, 0x1006);

			set_event(PSXINT_CDRLID, cdReadTime * 30);
			break;
		}
		else if (cdr.StatP & STATUS_ROTATING) {
			cdr.StatP &= ~STATUS_ROTATING;
		}
		else if (!(cdr_stat.Status & STATUS_SHELLOPEN)) {
			// closed now
			CheckCdrom();

			// cdr.StatP STATUS_SHELLOPEN is "sticky"
			// and is only cleared by CdlNop

			cdr.DriveState = DRIVESTATE_RESCAN_CD;
			set_event(PSXINT_CDRLID, cdReadTime * 105);
			break;
		}

		// recheck for close
		set_event(PSXINT_CDRLID, cdReadTime * 3);
		break;

	case DRIVESTATE_RESCAN_CD:
	    #ifdef DISP_DEBUG
        sprintf(txtbuffer, "cdrLidSeekInterrupt=DRIVESTATE_RESCAN_CD: %x ", cdr.StatP);
        DEBUG_print(txtbuffer, DBG_CDR4);
        #endif // DISP_DEBUG
		cdr.StatP |= STATUS_ROTATING;
		cdr.DriveState = DRIVESTATE_PREPARE_CD;

		// this is very long on real hardware, over 6 seconds
		// make it a bit faster here...
		set_event(PSXINT_CDRLID, cdReadTime * 150);
		break;

	case DRIVESTATE_PREPARE_CD:
	    #ifdef DISP_DEBUG
        sprintf(txtbuffer, "cdrLidSeekInterrupt=DRIVESTATE_PREPARE_CD: %x ", cdr.StatP);
        DEBUG_print(txtbuffer, DBG_CDR4);
        #endif // DISP_DEBUG
		if (cdr.StatP & STATUS_SEEK) {
			SetPlaySeekRead(cdr.StatP, 0);
			cdr.DriveState = DRIVESTATE_STANDBY;
		}
		else {
			SetPlaySeekRead(cdr.StatP, STATUS_SEEK);
			set_event(PSXINT_CDRLID, cdReadTime * 26);
		}
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

static int ReadTrack(const u8 *time)
{
	unsigned char tmp[4];
	int read_ok;

	tmp[0] = itob(time[0]);
	tmp[1] = itob(time[1]);
	tmp[2] = itob(time[2]);
	tmp[3] = 0;

	CDR_LOG("ReadTrack *** %02x:%02x:%02x\n", tmp[0], tmp[1], tmp[2]);

    if (*((u32*)cdr.Prev) == *((u32*)tmp)) {
		return 1;
    }

	read_ok = CDR_readTrack(tmp);
    if (read_ok)
	    *((u32*)cdr.Prev) = *((u32*)tmp);
    return read_ok;
}

static void UpdateSubq(const u8 *time)
{
	const struct SubQ *subq;
	int s = MSF2SECT_OLD(time[0], time[1], time[2]);
	u16 crc;

	if (CheckSBI(s))
		return;

	if (cdr.CurTrack == 1) {
		subq = (struct SubQ *)CDR_getBufferSub(s);
		if (subq != NULL )
		{
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
		else
		{
			generate_subq(time);
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

static void cdrPlayInterrupt_Autopause(s16* cddaBuf)
{
	u32 abs_lev_max = 0;
	bool abs_lev_chselect;
	u32 i;

	if ((cdr.Mode & MODE_AUTOPAUSE) && cdr.TrackChanged) {
		CDR_LOG_I("autopause\n");

		SetResultSize(1);
		cdr.Result[0] = cdr.StatP;
		setIrq(DataEnd, 0x1000); // 0x1000 just for logging purposes

		StopCdda();
		SetPlaySeekRead(cdr.StatP, 0);
		cdr.DriveState = DRIVESTATE_PAUSED;
	}
	else if ((cdr.Mode & MODE_REPORT) && !cdr.ReportDelay &&
		 ((cdr.subq.Absolute[2] & 0x0f) == 0 || cdr.FastForward || cdr.FastBackward))
	{
		SetResultSize(8);
		cdr.Result[0] = cdr.StatP;
		cdr.Result[1] = cdr.subq.Track;
		cdr.Result[2] = cdr.subq.Index;

		abs_lev_chselect = cdr.subq.Absolute[1] & 0x01;

		/* 8 is a hack. For accuracy, it should be 588. */
		for (i = 0; i < 8; i++)
		{
			abs_lev_max = MAX_VALUE(abs_lev_max, abs(cddaBuf[i * 2 + abs_lev_chselect]));
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

		setIrq(DataReady, 0x1001);
	}

	if (cdr.ReportDelay)
		cdr.ReportDelay--;
}

static int cdrSeekTime(unsigned char *target)
{
	int diff = msf2sec(cdr.SetSectorPlay) - msf2sec(target);
	int seekTime = abs(diff) * (cdReadTime / 2000);
	int cyclesSinceRS = psxRegs.cycle - cdr.LastReadSeekCycles;
	seekTime = MAX_VALUE(seekTime, 20000);

	// Transformers Beast Wars Transmetals does Setloc(x),SeekL,Setloc(x),ReadN
	// and then wants some slack time
	if (cdr.DriveState == DRIVESTATE_PAUSED || cyclesSinceRS < cdReadTime *3/2)
		seekTime += cdReadTime;

	seekTime = MIN_VALUE(seekTime, PSXCLK * 2 / 3);
	CDR_LOG("seek: %.2f %.2f (%.2f) st %d di %d\n", (float)seekTime / PSXCLK,
		(float)seekTime / cdReadTime, (float)cyclesSinceRS / cdReadTime,
		cdr.DriveState, diff);
	return seekTime;
}

static u32 cdrAlignTimingHack(u32 cycles)
{
	/*
	 * timing hack for T'ai Fu - Wrath of the Tiger:
	 * The game has a bug where it issues some cdc commands from a low priority
	 * vint handler, however there is a higher priority default bios handler
	 * that acks the vint irq and returns, so game's handler is not reached
	 * (see bios irq handler chains at e004 and the game's irq handling func
	 * at 80036810). For the game to work, vint has to arrive after the bios
	 * vint handler rejects some other irq (of which only cd and rcnt2 are
	 * active), but before the game's handler loop reads I_STAT. The time
	 * window for this is quite small (~1k cycles of so). Apparently this
	 * somehow happens naturally on the real hardware.
	 *
	 * Note: always enforcing this breaks other games like Crash PAL version
	 * (inputs get dropped because bios handler doesn't see interrupts).
	 */
	u32 vint_rel;
	if (psxRegs.cycle - rcnts[3].cycleStart > 250000)
		return cycles;
	vint_rel = rcnts[3].cycleStart + 63000 - psxRegs.cycle;
	vint_rel += PSXCLK / 60;
	while ((s32)(vint_rel - cycles) < 0)
		vint_rel += PSXCLK / 60;
	return vint_rel;
}

static void cdrUpdateTransferBuf(const u8 *buf);
static void cdrReadInterrupt(void);
static void cdrPrepCdda(s16 *buf, int samples);

static void msfiAdd(u8 *msfi, u32 count)
{
	assert(count < 75);
	msfi[2] += count;
	if (msfi[2] >= 75) {
		msfi[2] -= 75;
		msfi[1]++;
		if (msfi[1] == 60) {
			msfi[1] = 0;
			msfi[0]++;
		}
	}
}

static void msfiSub(u8 *msfi, u32 count)
{
	assert(count < 75);
	msfi[2] -= count;
	if ((s8)msfi[2] < 0) {
		msfi[2] += 75;
		msfi[1]--;
		if ((s8)msfi[1] < 0) {
			msfi[1] = 60;
			msfi[0]--;
		}
	}
}

void cdrPlayReadInterrupt(void)
{
	cdr.LastReadSeekCycles = psxRegs.cycle;

	if (cdr.Reading) {
		cdrReadInterrupt();
		return;
	}

	if (!cdr.Play) return;

	CDR_LOG("CDDA - %02d:%02d:%02d m %02x\n",
		cdr.SetSectorPlay[0], cdr.SetSectorPlay[1], cdr.SetSectorPlay[2], cdr.Mode);

	cdr.DriveState = DRIVESTATE_PLAY_READ;
    SetPlaySeekRead(cdr.StatP, STATUS_PLAY);
	//if (memcmp(cdr.SetSectorPlay, cdr.SetSectorEnd, 3) == 0) {
	if (*(u32 *)cdr.SetSectorPlay >= *(u32 *)cdr.SetSectorEnd) {
		CDR_LOG_I("end stop\n");
		StopCdda();
		SetPlaySeekRead(cdr.StatP, 0);
		cdr.TrackChanged = TRUE;
		cdr.DriveState = DRIVESTATE_PAUSED;
	}
	else {
		CDR_readCDDA(cdr.SetSectorPlay[0], cdr.SetSectorPlay[1], cdr.SetSectorPlay[2], (u8 *)read_buf);
	}

	if (!cdr.IrqStat && (cdr.Mode & (MODE_AUTOPAUSE|MODE_REPORT)))
	{
		// update for CdlGetlocP/autopause
	    generate_subq(cdr.SetSectorPlay);

	    cdrPlayInterrupt_Autopause(read_buf);
	}

	//if (!cdr.Play) return;

	if (cdr.Play && !Config.Cdda) {
        cdrPrepCdda(read_buf, CD_FRAMESIZE_RAW / 4);
		SPU_playCDDAchannel(read_buf, CD_FRAMESIZE_RAW, psxRegs.cycle, 0);
	}

	msfiAdd(cdr.SetSectorPlay, 1);

	// Backup location information for CdlGetlocP
	*((u32*)cdr.SetSectorPlayBak) = *((u32*)cdr.SetSectorPlay);

	CDRPLAYREAD_INT(cdReadTime, 0);
}

static void softReset(void)
{
	CDR_getStatus(&cdr_stat);
	if (cdr_stat.Status & STATUS_SHELLOPEN) {
		cdr.DriveState = DRIVESTATE_LID_OPEN;
		cdr.StatP = STATUS_SHELLOPEN;
	}
	else if (CdromId[0] == '\0') {
		cdr.DriveState = DRIVESTATE_STOPPED;
		cdr.StatP = 0;
	}
	else {
		cdr.DriveState = DRIVESTATE_STANDBY;
		cdr.StatP = STATUS_ROTATING;
	}

	cdr.FifoOffset = DATA_SIZE; // fifo empty
	cdr.LocL[0] = LOCL_INVALID;
	cdr.Mode = MODE_SIZE_2340;
	cdr.Muted = FALSE;
	SPU_setCDvol(cdr.AttenuatorLeftToLeft, cdr.AttenuatorLeftToRight,
		cdr.AttenuatorRightToLeft, cdr.AttenuatorRightToRight, psxRegs.cycle);
}

#define CMD_PART2           0x100
#define CMD_WHILE_NOT_READY 0x200

void cdrInterrupt(void) {
	int start_rotating = 0;
	int error = 0;
	u32 cycles, seekTime = 0;
	u32 second_resp_time = 0;
	const void *buf;
	u8 ParamC;
	u8 set_loc[4];
	int read_ok;
	u16 not_ready = 0;
	u8 IrqStat = Acknowledge;
	u8 DriveStateOld;
	u16 Cmd;
	int i;

	if (cdr.IrqStat) {
		CDR_LOG_I("cmd %02x with irqstat %x\n",
			cdr.CmdInProgress, cdr.IrqStat);
		return;
	}
	if (cdr.Irq1Pending) {
		// hand out the "newest" sector, according to nocash
		cdrUpdateTransferBuf(CDR_getBuffer());
		CDR_LOG_I("%x:%02x:%02x loaded on ack, cmd=%02x res=%02x\n",
			cdr.Transfer[0], cdr.Transfer[1], cdr.Transfer[2],
			cdr.CmdInProgress, cdr.Irq1Pending);
		SetResultSize(1);
		cdr.Result[0] = cdr.Irq1Pending;
		cdr.Irq1Pending = 0;
		setIrq((cdr.Irq1Pending & STATUS_ERROR) ? DiskError : DataReady, 0x1003);
		return;
	}

	// default response
	SetResultSize(1);
	cdr.Result[0] = cdr.StatP;

	Cmd = cdr.CmdInProgress;
	cdr.CmdInProgress = 0;
	ParamC = cdr.ParamC;

	if (Cmd < 0x100) {
		cdr.Ctrl &= ~0x80;
		cdr.ParamC = 0;
		cdr.Cmd = 0;
		#ifdef DISP_DEBUG
		//sprintf(txtbuffer, "cdrInterrupt1  %s ", CmdName[Cmd]);
		//DEBUG_print(txtbuffer, DBG_CDR1);
		#endif // DISP_DEBUG
	}
	else
	{
		#ifdef DISP_DEBUG
		//sprintf(txtbuffer, "cdrInterrupt2  %s ", CmdName[Cmd - CMD_PART2]);
		//DEBUG_print(txtbuffer, DBG_CDR2);
		#endif // DISP_DEBUG
	}

	switch (cdr.DriveState) {
	case DRIVESTATE_PREPARE_CD:
		if (Cmd > 2) {
			// Syphon filter 2 expects commands to work shortly after it sees
			// STATUS_ROTATING, so give up trying to emulate the startup seq
			cdr.DriveState = DRIVESTATE_STANDBY;
			cdr.StatP &= ~STATUS_SEEK;
			psxRegs.interrupt &= ~(1 << PSXINT_CDRLID);
			break;
		}
		// fallthrough
	case DRIVESTATE_LID_OPEN:
	case DRIVESTATE_RESCAN_CD:
		// no disk or busy with the initial scan, allowed cmds are limited
		not_ready = CMD_WHILE_NOT_READY;
		break;
	}

	switch (Cmd | not_ready) {
		case CdlNop:
		case CdlNop + CMD_WHILE_NOT_READY:
			if (cdr.DriveState != DRIVESTATE_LID_OPEN)
				cdr.StatP &= ~STATUS_SHELLOPEN;
			break;

		case CdlSetloc:
		// case CdlSetloc + CMD_WHILE_NOT_READY: // or is it?
			CDR_LOG("CDROM setloc command (%02X, %02X, %02X)\n", cdr.Param[0], cdr.Param[1], cdr.Param[2]);

			// MM must be BCD, SS must be BCD and <0x60, FF must be BCD and <0x75
			if (((cdr.Param[0] & 0x0F) > 0x09) || (cdr.Param[0] > 0x99) || ((cdr.Param[1] & 0x0F) > 0x09) || (cdr.Param[1] >= 0x60) || ((cdr.Param[2] & 0x0F) > 0x09) || (cdr.Param[2] >= 0x75))
			{
				CDR_LOG_I("Invalid/out of range seek to %02X:%02X:%02X\n", cdr.Param[0], cdr.Param[1], cdr.Param[2]);
				if (++cdr.errorRetryhack > 100)
					break;
				error = ERROR_BAD_ARGNUM;
				goto set_error;
			}
			else
			{
				set_loc[0] = btoi(cdr.Param[0]);
				set_loc[1] = btoi(cdr.Param[1]);
				set_loc[2] = btoi(cdr.Param[2]);
				set_loc[3] = 0;
				*((u32*)cdr.SetSector) = *((u32*)set_loc);
				cdr.SetlocPending = 1;
				cdr.errorRetryhack = 0;
			}
			break;

		do_CdlPlay:
		case CdlPlay:
			StopCdda();
			StopReading();

			cdr.FastBackward = 0;
			cdr.FastForward = 0;

			// BIOS CD Player
			// - Pause player, hit Track 01/02/../xx (Setloc issued!!)

			if (ParamC != 0 && cdr.Param[0] != 0) {
				int track = btoi( cdr.Param[0] );

				if (track <= cdr.ResultTN[1])
					cdr.CurTrack = track;

				CDR_LOG("PLAY track %d\n", cdr.CurTrack);

				if (CDR_getTD((u8)cdr.CurTrack, cdr.ResultTD) != -1) {
					set_loc[0] = cdr.ResultTD[2];
				    set_loc[1] = cdr.ResultTD[1];
					set_loc[2] = cdr.ResultTD[0];
					set_loc[3] = 0;

					seekTime = cdrSeekTime(set_loc);
					*((u32*)cdr.SetSectorPlay) = *((u32*)set_loc);
				}
			}
			else if (cdr.SetlocPending) {
				seekTime = cdrSeekTime(cdr.SetSector);
				//memcpy(cdr.SetSectorPlay, cdr.SetSector, 4);
				*((u32*)cdr.SetSectorPlay) = *((u32*)cdr.SetSector);
			}
			else {
				CDR_LOG("PLAY Resume @ %d:%d:%d\n",
					cdr.SetSectorPlay[0], cdr.SetSectorPlay[1], cdr.SetSectorPlay[2]);
			}
			cdr.SetlocPending = 0;

			/*
			Rayman: detect track changes
			- fixes logo freeze

			Twisted Metal 2: skip PREGAP + starting accurate SubQ
			- plays tracks without retry play

			Wild 9: skip PREGAP + starting accurate SubQ
			- plays tracks without retry play
			*/
			Find_CurTrack(cdr.SetSectorPlay);
			generate_subq(cdr.SetSectorPlay);
			cdr.LocL[0] = LOCL_INVALID;
			cdr.SubqForwardSectors = 1;
			cdr.TrackChanged = FALSE;
			cdr.FileChannelSelected = 0;
			cdr.AdpcmActive = 0;
			cdr.ReportDelay = 60;
			cdr.sectorsRead = 0;

			if (!Config.Cdda)
				CDR_play(cdr.SetSectorPlay);

			SetPlaySeekRead(cdr.StatP, STATUS_SEEK | STATUS_ROTATING);

			// BIOS player - set flag again
			cdr.Play = TRUE;
			cdr.DriveState = DRIVESTATE_PLAY_READ;

			CDRPLAYREAD_INT(cdReadTime + seekTime, 1);
			start_rotating = 1;
			break;

		case CdlForward:
			// TODO: error 80 if stopped
			IrqStat = Complete;

			// GameShark CD Player: Calls 2x + Play 2x
			cdr.FastForward = 1;
			cdr.FastBackward = 0;
			break;

		case CdlBackward:
			IrqStat = Complete;

			// GameShark CD Player: Calls 2x + Play 2x
			cdr.FastBackward = 1;
			cdr.FastForward = 0;
			break;

		case CdlStandby:
			if (cdr.DriveState != DRIVESTATE_STOPPED) {
				error = ERROR_BAD_ARGNUM;
				goto set_error;
			}
			cdr.DriveState = DRIVESTATE_STANDBY;
			second_resp_time = cdReadTime * 125 / 2;
			start_rotating = 1;
			break;

		case CdlStandby + CMD_PART2:
			IrqStat = Complete;
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
			SetPlaySeekRead(cdr.StatP, 0);
			cdr.StatP &= ~STATUS_ROTATING;
			cdr.LocL[0] = LOCL_INVALID;

			second_resp_time = 0x800;
			if (cdr.DriveState != DRIVESTATE_STOPPED)
				second_resp_time = cdReadTime * 30 / 2;

			cdr.DriveState = DRIVESTATE_STOPPED;
			break;

		case CdlStop + CMD_PART2:
			IrqStat = Complete;
			break;

		case CdlPause:
			if (cdr.AdpcmActive) {
				cdr.AdpcmActive = 0;
				cdr.Xa.nsamples = 0;
				SPU_playADPCMchannel(&cdr.Xa, psxRegs.cycle, 1); // flush adpcm
			}
			StopCdda();
			StopReading();

			// how the drive maintains the position while paused is quite
			// complicated, this is the minimum to make "Bedlam" happy
			msfiSub(cdr.SetSectorPlay, MIN_VALUE(cdr.sectorsRead, 4));
			cdr.sectorsRead = 0;

			/*
			Gundam Battle Assault 2
			Hokuto no Ken 2
			InuYasha - Feudal Fairy Tale
			Dance Dance Revolution Konamix
			...
			*/
			if (!(cdr.StatP & (STATUS_PLAY | STATUS_READ)))
			{
				second_resp_time = 7000;
			}
			else
			{
				if (fastLoad)
				{
					second_resp_time = cdReadTime;
				}
				else
				{
					second_resp_time = 2 * 1097107;
				}
			}
			SetPlaySeekRead(cdr.StatP, 0);
			DriveStateOld = cdr.DriveState;
			cdr.DriveState = DRIVESTATE_PAUSED;
			if (DriveStateOld == DRIVESTATE_SEEK) {
				// According to Duckstation this fails, but the
				// exact conditions and effects are not clear.
				// Moto Racer World Tour seems to rely on this.
				// For now assume pause works anyway, just errors out.
				error = ERROR_NOTREADY;
				goto set_error;
			}
			break;

		case CdlPause + CMD_PART2:
			IrqStat = Complete;
			break;

		case CdlReset:
		case CdlReset + CMD_WHILE_NOT_READY:
			// note: nocash and Duckstation calls this 'Init', but
			// the official SDK calls it 'Reset', and so do we
			StopCdda();
			StopReading();
			softReset();
			second_resp_time = not_ready ? 70000 : 4100000;
			start_rotating = 1;
			break;

		case CdlReset + CMD_PART2:
		case CdlReset + CMD_PART2 + CMD_WHILE_NOT_READY:
			IrqStat = Complete;
			break;

		case CdlMute:
			cdr.Muted = TRUE;
			SPU_setCDvol(0, 0, 0, 0, psxRegs.cycle);
			break;

		case CdlDemute:
			cdr.Muted = FALSE;
			SPU_setCDvol(cdr.AttenuatorLeftToLeft, cdr.AttenuatorLeftToRight,
				cdr.AttenuatorRightToLeft, cdr.AttenuatorRightToRight, psxRegs.cycle);
			break;

		case CdlSetfilter:
			cdr.FilterFile = cdr.Param[0];
			cdr.FilterChannel = cdr.Param[1];
			cdr.FileChannelSelected = 0;
			break;

		case CdlSetmode:
		case CdlSetmode + CMD_WHILE_NOT_READY:
			CDR_LOG("cdrWrite1() Log: Setmode %x\n", cdr.Param[0]);
			cdr.Mode = cdr.Param[0];
			break;

		case CdlGetparam:
		case CdlGetparam + CMD_WHILE_NOT_READY:
			/* Gameblabla : According to mednafen, Result size should be 5 and done this way. */
			SetResultSize_(5);
			cdr.Result[1] = cdr.Mode;
			cdr.Result[2] = 0;
			cdr.Result[3] = cdr.FilterFile;
			cdr.Result[4] = cdr.FilterChannel;
			break;

		case CdlGetlocL:
			if (cdr.LocL[0] == LOCL_INVALID) {
				error = 0x80;
				goto set_error;
			}
			SetResultSize_(8);
			//memcpy(cdr.Result, cdr.LocL, 8);
			*(long long int *)(&cdr.Result) = *(long long int *)(&cdr.LocL);
			break;

		case CdlGetlocP:
			#ifdef DISP_DEBUG
			//sprintf(txtbuffer, "CdlGetlocP ");
			//DEBUG_print(txtbuffer, DBG_CDR2);
			#endif // DISP_DEBUG
			SetResultSize_(8);
			//memcpy(&cdr.Result, &cdr.subq, 8);
			UpdateSubq(cdr.SetSectorPlayBak);
			*(long long int *)(&cdr.Result) = *(long long int *)(&cdr.subq);
			break;

		case CdlReadT: // SetSession?
			// really long
			second_resp_time = cdReadTime * 290 / 4;
			start_rotating = 1;
			break;

		case CdlReadT + CMD_PART2:
			IrqStat = Complete;
			break;

		case CdlGetTN:
			if (CDR_getTN(cdr.ResultTN) == -1) {
				assert(0);
			}
			SetResultSize_(3);
			cdr.Result[1] = itob(cdr.ResultTN[0]);
			cdr.Result[2] = itob(cdr.ResultTN[1]);
			break;

		case CdlGetTD:
			cdr.Track = btoi(cdr.Param[0]);
			if (CDR_getTD(cdr.Track, cdr.ResultTD) == -1) {
				error = ERROR_BAD_ARGVAL;
				goto set_error;
			}
			SetResultSize_(3);
			cdr.Result[1] = itob(cdr.ResultTD[2]);
			cdr.Result[2] = itob(cdr.ResultTD[1]);
			// no sector number
			//cdr.Result[3] = itob(cdr.ResultTD[0]);
			break;

		case CdlSeekL:
		case CdlSeekP:
			StopCdda();
			StopReading();
			SetPlaySeekRead(cdr.StatP, STATUS_SEEK | STATUS_ROTATING);

			if (fastLoad)
			{
				seekTime = WaitTime1st;
			}
			else
			{
				seekTime = cdrSeekTime(cdr.SetSector);
			}
			*((u32*)cdr.SetSectorPlay) = *((u32*)cdr.SetSector);
			cdr.DriveState = DRIVESTATE_SEEK;
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
            second_resp_time = cdReadTime + seekTime;
			start_rotating = 1;
			break;

		case CdlSeekL + CMD_PART2:
		case CdlSeekP + CMD_PART2:
			SetPlaySeekRead(cdr.StatP, 0);
			cdr.Result[0] = cdr.StatP;
			IrqStat = Complete;

			Find_CurTrack(cdr.SetSectorPlay);
			read_ok = ReadTrack(cdr.SetSectorPlay);
			if (read_ok && (buf = CDR_getBuffer()))
				//memcpy(cdr.LocL, buf, 8);
				*(long long int *)(&cdr.LocL) = *(long long int *)(buf);
			//UpdateSubq(cdr.SetSectorPlay);
			*((u32*)cdr.SetSectorPlayBak) = *((u32*)cdr.SetSectorPlay);
			cdr.DriveState = DRIVESTATE_STANDBY;
			cdr.TrackChanged = FALSE;
			cdr.LastReadSeekCycles = psxRegs.cycle;
			break;

		case CdlTest:
		case CdlTest + CMD_WHILE_NOT_READY:
			switch (cdr.Param[0]) {
				case 0x20: // System Controller ROM Version
					SetResultSize_(4);
					memcpy(cdr.Result, Test20, 4);
					break;
				case 0x22:
					SetResultSize_(8);
					memcpy(cdr.Result, Test22, 4);
					break;
				case 0x23: case 0x24:
					SetResultSize_(8);
					memcpy(cdr.Result, Test23, 4);
					break;
			}
			break;

		case CdlID:
			second_resp_time = 20480;
			break;

		case CdlID + CMD_PART2:
			SetResultSize_(8);
			cdr.Result[0] = cdr.StatP;
			cdr.Result[1] = 0;
			cdr.Result[2] = 0;
			cdr.Result[3] = 0;

			extern bool executingBios;
			// 0x10 - audio | 0x40 - disk missing | 0x80 - unlicensed
			if (executingBios || CDR_getStatus(&cdr_stat) == -1 || cdr_stat.Type == 0 || cdr_stat.Type == 0xff) {
				cdr.Result[1] = 0xc0;
			}
			else {
				if (cdr_stat.Type == 2)
					cdr.Result[1] |= 0x10;
				if (CdromId[0] == '\0')
					cdr.Result[1] |= 0x80;
			}
			cdr.Result[0] |= (cdr.Result[1] >> 4) & 0x08;
			CDR_LOG_I("CdlID: %02x %02x %02x %02x\n", cdr.Result[0],
				cdr.Result[1], cdr.Result[2], cdr.Result[3]);

			/* This adds the string "PCSX" in Playstation bios boot screen */
			//memcpy((char *)&cdr.Result[4], "PCSX", 4);
#ifdef HW_RVL
			strncpy((char *)&cdr.Result[4], "WSX ", 4);
#else
			strncpy((char *)&cdr.Result[4], "GCSX", 4);
#endif
			IrqStat = Complete;
			break;

		case CdlInit:
		case CdlInit + CMD_WHILE_NOT_READY:
			StopCdda();
			StopReading();
			SetPlaySeekRead(cdr.StatP, 0);
			// yes, it really sets STATUS_SHELLOPEN
			cdr.StatP |= STATUS_SHELLOPEN;
			cdr.DriveState = DRIVESTATE_RESCAN_CD;
			set_event(PSXINT_CDRLID, 20480);
			start_rotating = 1;
			break;

		case CdlGetQ:
		case CdlGetQ + CMD_WHILE_NOT_READY:
			break;

		case CdlReadToc:
		case CdlReadToc + CMD_WHILE_NOT_READY:
			cdr.LocL[0] = LOCL_INVALID;
			second_resp_time = cdReadTime * 180 / 4;
			start_rotating = 1;
			break;

		case CdlReadToc + CMD_PART2:
		case CdlReadToc + CMD_PART2 + CMD_WHILE_NOT_READY:
			IrqStat = Complete;
			break;

		case CdlReadN:
		case CdlReadS:
			if (cdr.Reading && !cdr.SetlocPending)
			{
				break;
			}

			Find_CurTrack(cdr.SetlocPending ? cdr.SetSector : cdr.SetSectorPlay);

			if ((cdr.Mode & MODE_CDDA) && cdr.CurTrack > 1)
				// Read* acts as play for cdda tracks in cdda mode
				goto do_CdlPlay;

			StopCdda();
			if (cdr.SetlocPending) {
				#ifdef DISP_DEBUG
				//static int cdrSeekTimeCnt = 0;
				//sprintf(txtbuffer, "CdlRead cdrSeekTime %d ", cdrSeekTimeCnt++);
				//DEBUG_print(txtbuffer, DBG_CDR1);
				#endif // DISP_DEBUG
				seekTime = cdrSeekTime(cdr.SetSector);
				*((u32*)cdr.SetSectorPlay) = *((u32*)cdr.SetSector);
				cdr.SetlocPending = 0;
			}
			cdr.Reading = 1;
			cdr.FileChannelSelected = 0;
			cdr.AdpcmActive = 0;

			// Fighting Force 2 - update subq time immediately
			// - fixes new game
			//UpdateSubq(cdr.SetSectorPlay);
			*((u32*)cdr.SetSectorPlayBak) = *((u32*)cdr.SetSectorPlay);
			cdr.LocL[0] = LOCL_INVALID;
			cdr.SubqForwardSectors = 1;
			cdr.sectorsRead = 0;
			cdr.DriveState = DRIVESTATE_SEEK;

			cycles = (cdr.Mode & MODE_SPEED) ? cdReadTime : cdReadTime * 2;
			if (fastLoad)
			{
				if (seekTime > MinSeekTime)
				{
					seekTime = MinSeekTime;
				}
			}
			cycles += seekTime;
			if (Config.hacks.cdr_read_timing)
				cycles = cdrAlignTimingHack(cycles);
			CDRPLAYREAD_INT(cycles, 1);

			SetPlaySeekRead(cdr.StatP, STATUS_SEEK);
			start_rotating = 1;
			break;

		case CdlSync:
		default:
			error = ERROR_INVALIDCMD;
			// FALLTHROUGH

		set_error:
			SetResultSize_(2);
			cdr.Result[0] = cdr.StatP | STATUS_ERROR;
			cdr.Result[1] = not_ready ? ERROR_NOTREADY : error;
			IrqStat = DiskError;
			CDR_LOG_I("cmd %02x error %02x\n", Cmd, cdr.Result[1]);
			break;
	}

	if (cdr.DriveState == DRIVESTATE_STOPPED && start_rotating) {
		cdr.DriveState = DRIVESTATE_STANDBY;
		cdr.StatP |= STATUS_ROTATING;
	}

	if (second_resp_time) {
		cdr.CmdInProgress = Cmd | 0x100;
		set_event(PSXINT_CDR, second_resp_time);
	}
	else if (cdr.Cmd && cdr.Cmd != (Cmd & 0xff)) {
		cdr.CmdInProgress = cdr.Cmd;
		CDR_LOG_I("cmd %02x came before %02x finished\n", cdr.Cmd, Cmd);
	}

	setIrq(IrqStat, Cmd);
}

#ifdef HAVE_ARMV7
 #define ssat32_to_16(v) \
  asm("ssat %0,#16,%1" : "=r" (v) : "r" (v))
#else
 #define ssat32_to_16(v) do { \
  if (v < -32768) v = -32768; \
  else if (v > 32767) v = 32767; \
 } while (0)
#endif

static void cdrPrepCdda(s16 *buf, int samples)
{
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	int i;
	for (i = 0; i < samples; i++) {
		//buf[i * 2 + 0] = SWAP16(buf[i * 2 + 0]);
		//buf[i * 2 + 1] = SWAP16(buf[i * 2 + 1]);
		buf[i * 2 + 0] = buf[i * 2 + 0];
		buf[i * 2 + 1] = buf[i * 2 + 1];
	}
#endif
}

static void cdrReadInterruptSetResult(unsigned char result)
{
	if (cdr.IrqStat) {
		CDR_LOG_I("%d:%02d:%02d irq miss, cmd=%02x irqstat=%02x\n",
			cdr.SetSectorPlay[0], cdr.SetSectorPlay[1], cdr.SetSectorPlay[2],
			cdr.CmdInProgress, cdr.IrqStat);
		cdr.Irq1Pending = result;
		return;
	}
	SetResultSize(1);
	cdr.Result[0] = result;
	setIrq((result & STATUS_ERROR) ? DiskError : DataReady, 0x1004);
}

static void cdrUpdateTransferBuf(const u8 *buf)
{
	if (!buf)
		return;
	memcpy(cdr.Transfer, buf, DATA_SIZE);
	CheckPPFCache(cdr.Transfer, cdr.Prev[0], cdr.Prev[1], cdr.Prev[2]);
	CDR_LOG("cdr.Transfer  %02x:%02x:%02x\n",
		cdr.Transfer[0], cdr.Transfer[1], cdr.Transfer[2]);
	if (cdr.FifoOffset < 2048 + 12)
		CDR_LOG("FifoOffset(1) %d/%d\n", cdr.FifoOffset, cdr.FifoSize);
}

static void cdrReadInterrupt(void)
{
	const struct { u8 file, chan, mode, coding; } *subhdr;
	const u8 *buf = NULL;
	int deliver_data = 1;
	u8 subqPos[4];
	int read_ok;
	int is_start;

	*((u32*)subqPos) = *((u32*)cdr.SetSectorPlay);
	msfiAdd(subqPos, cdr.SubqForwardSectors);
	//UpdateSubq(subqPos);
	*((u32*)cdr.SetSectorPlayBak) = *((u32*)subqPos);
	if (cdr.SubqForwardSectors < SUBQ_FORWARD_SECTORS) {
		cdr.SubqForwardSectors++;
		CDRPLAYREAD_INT((cdr.Mode & MODE_SPEED) ? (cdReadTime / 2) : cdReadTime, 0);
		return;
	}

	// note: CdlGetlocL should work as soon as STATUS_READ is indicated
	SetPlaySeekRead(cdr.StatP, STATUS_READ | STATUS_ROTATING);
	cdr.DriveState = DRIVESTATE_PLAY_READ;
	cdr.sectorsRead++;

	read_ok = ReadTrack(cdr.SetSectorPlay);
	if (read_ok)
		buf = CDR_getBuffer();
	if (buf == NULL)
		read_ok = 0;

	if (!read_ok) {
		CDR_LOG_I("cdrReadInterrupt() Log: err\n");
		cdrReadInterruptSetResult(cdr.StatP | STATUS_ERROR);
		cdr.DriveState = DRIVESTATE_PAUSED; // ?
		return;
	}
	//memcpy(cdr.LocL, buf, 8);
	*(long long int *)(&cdr.LocL) = *(long long int *)(buf);

	if (!cdr.IrqStat && !cdr.Irq1Pending)
		cdrUpdateTransferBuf(buf);

	subhdr = (void *)(buf + 4);
	do {
		// try to process as adpcm
		if (!(cdr.Mode & MODE_STRSND))
			break;
		if (buf[3] != 2 || (subhdr->mode & 0x44) != 0x44) // or 0x64?
			break;
		CDR_LOG("f=%d m=%d %d,%3d | %d,%2d | %d,%2d\n", !!(cdr.Mode & MODE_SF), cdr.Muted,
			subhdr->file, subhdr->chan, cdr.CurFile, cdr.CurChannel, cdr.FilterFile, cdr.FilterChannel);
		if ((cdr.Mode & MODE_SF) && (subhdr->file != cdr.FilterFile || subhdr->chan != cdr.FilterChannel))
			break;
		if (subhdr->chan & 0x80) { // ?
			if (subhdr->chan != 0xff)
				log_unhandled("adpcm %d:%d\n", subhdr->file, subhdr->chan);
			break;
		}
		if (!cdr.FileChannelSelected) {
			cdr.CurFile = subhdr->file;
			cdr.CurChannel = subhdr->chan;
			cdr.FileChannelSelected = 1;
		}
		else if (subhdr->file != cdr.CurFile || subhdr->chan != cdr.CurChannel)
			break;

		// accepted as adpcm
		deliver_data = 0;

		if (Config.Xa)
			break;
		is_start = !cdr.AdpcmActive;
		cdr.AdpcmActive = !xa_decode_sector(&cdr.Xa, buf + 4, is_start);
		if (cdr.AdpcmActive)
			SPU_playADPCMchannel(&cdr.Xa, psxRegs.cycle, is_start);
	} while (0);

	if ((cdr.Mode & MODE_SF) && (subhdr->mode & 0x44) == 0x44) // according to nocash
		deliver_data = 0;

	/*
	Croc 2: $40 - only FORM1 (*)
	Judge Dredd: $C8 - only FORM1 (*)
	Sim Theme Park - no adpcm at all (zero)
	*/

	if (deliver_data)
		cdrReadInterruptSetResult(cdr.StatP);

	msfiAdd(cdr.SetSectorPlay, 1);

	CDRPLAYREAD_INT((cdr.Mode & MODE_SPEED) ? (cdReadTime / 2) : cdReadTime, 0);
}

/*
cdrRead0:
	bit 0,1 - reg index
	bit 2 - adpcm active
	bit 5 - 1 result ready
	bit 6 - 1 dma ready
	bit 7 - 1 command being processed
*/

unsigned char cdrRead0(void) {
	cdr.Ctrl &= ~0x24;
	cdr.Ctrl |= cdr.AdpcmActive << 2;
	cdr.Ctrl |= cdr.ResultReady << 5;

	cdr.Ctrl |= 0x40; // data fifo not empty

	// What means the 0x10 and the 0x08 bits? I only saw it used by the bios
	cdr.Ctrl |= 0x18;

	CDR_LOG_IO("cdr r0.sta: %02x\n", cdr.Ctrl);

	return psxHu8(0x1800) = cdr.Ctrl;
}

void cdrWrite0(unsigned char rt) {
	CDR_LOG_IO("cdr w0.idx: %02x\n", rt);

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

	CDR_LOG_IO("cdr r1.rsp: %02x #%u\n", psxHu8(0x1801), cdr.ResultP - 1);

	return psxHu8(0x1801);
}

void cdrWrite1(unsigned char rt) {
	//const char *rnames[] = { "cmd", "smd", "smc", "arr" }; (void)rnames;
	//CDR_LOG_IO("cdr w1.%s: %02x\n", rnames[cdr.Ctrl & 3], rt);

	switch (cdr.Ctrl & 3) {
	case 0:
		break;
	case 3:
		cdr.AttenuatorRightToRightT = rt;
		return;
	default:
		return;
	}

#ifdef CDR_LOG_CMD_IRQ
	CDR_LOG_I("CD1 write: %x (%s)", rt, CmdName[rt]);
	if (cdr.ParamC) {
		int i;
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

	if (!cdr.CmdInProgress) {
		#ifdef DISP_DEBUG
		sprintf(txtbuffer, "cdrCmd1 %s \r\n", CmdName[rt]);
		DEBUG_print(txtbuffer, DBG_CDR1);
		//writeLogFile(txtbuffer);
		#endif // DISP_DEBUG
		cdr.CmdInProgress = rt;
		if (rt == CdlPause)
		{
			set_event(PSXINT_CDR, 27648);
		}
		else
		{
			// should be something like 12k + controller delays
		    set_event(PSXINT_CDR, fastLoad ? WaitTime1st : 5000);
		}
	}
	else {
		CDR_LOG_I("cmd while busy: %02x, prev %02x, busy %02x\n",
			rt, cdr.Cmd, cdr.CmdInProgress);
		#ifdef DISP_DEBUG
		sprintf(txtbuffer, "cdrCmd2 %s %s \r\n", CmdName[rt], CmdName[cdr.CmdInProgress < 0x100 ? cdr.CmdInProgress : cdr.CmdInProgress - CMD_PART2]);
		DEBUG_print(txtbuffer, DBG_CDR2);
		//writeLogFile(txtbuffer);
		#endif // DISP_DEBUG
		if (cdr.CmdInProgress < 0x100) // no pending 2nd response
			cdr.CmdInProgress = rt;
	}

	cdr.Cmd = rt;
}

unsigned char cdrRead2(void) {
	unsigned char ret = cdr.Transfer[0x920];

	if (cdr.FifoOffset < cdr.FifoSize)
		ret = cdr.Transfer[cdr.FifoOffset++];
	else
		CDR_LOG_I("read empty fifo (%d)\n", cdr.FifoSize);

	CDR_LOG_IO("cdr r2.dat: %02x\n", ret);
	return ret;
}

void cdrWrite2(unsigned char rt) {
	//const char *rnames[] = { "prm", "ien", "all", "arl" }; (void)rnames;
	//CDR_LOG_IO("cdr w2.%s: %02x\n", rnames[cdr.Ctrl & 3], rt);

	switch (cdr.Ctrl & 3) {
	case 0:
		if (cdr.ParamC < 8) // FIXME: size and wrapping
			cdr.Param[cdr.ParamC++] = rt;
		return;
	case 1:
		cdr.IrqMask = rt;
		setIrq(cdr.IrqStat, 0x1005);
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
		psxHu8(0x1803) = cdr.IrqStat | 0xE0;
	else
		psxHu8(0x1803) = cdr.IrqMask | 0xE0;

	CDR_LOG_IO("cdr r3.%s: %02x\n", (cdr.Ctrl & 1) ? "ifl" : "ien", psxHu8(0x1803));
	return psxHu8(0x1803);
}

void cdrWrite3(unsigned char rt) {
	//const char *rnames[] = { "req", "ifl", "alr", "ava" }; (void)rnames;
    u8 ll, lr, rl, rr;
	//CDR_LOG_IO("cdr w3.%s: %02x\n", rnames[cdr.Ctrl & 3], rt);

	switch (cdr.Ctrl & 3) {
	case 0:
		break; // transfer
	case 1:
		if (cdr.IrqStat & rt) {
			u32 nextCycle = psxRegs.intCycle[PSXINT_CDR].sCycle
				+ psxRegs.intCycle[PSXINT_CDR].cycle;
			int pending = psxRegs.interrupt & (1 << PSXINT_CDR);
#ifdef CDR_LOG_CMD_IRQ
			CDR_LOG_I("ack %02x (w=%02x p=%d,%x,%x,%d)\n",
				cdr.IrqStat & rt, rt, !!pending, cdr.CmdInProgress,
				cdr.Irq1Pending, nextCycle - psxRegs.cycle);
#endif
			// note: Croc, Shadow Tower (more) vs Discworld Noir (<993)
			if (!pending && (cdr.CmdInProgress || cdr.Irq1Pending))
			{
				s32 c = 2048;
				if (cdr.CmdInProgress) {
					c = 2048 - (psxRegs.cycle - nextCycle);
					c = MAX_VALUE(c, 512);
				}
				set_event(PSXINT_CDR, c);
			}
		}
		cdr.IrqStat &= ~rt;

		if (rt & 0x40)
			cdr.ParamC = 0;
		return;
	case 2:
		cdr.AttenuatorLeftToRightT = rt;
		return;
	case 3:
		if (rt & 0x01)
			log_unhandled("Mute ADPCM?\n");
		if (rt & 0x20) {
			ll = cdr.AttenuatorLeftToLeftT; lr = cdr.AttenuatorLeftToRightT;
			rl = cdr.AttenuatorRightToLeftT; rr = cdr.AttenuatorRightToRightT;
			if (ll == cdr.AttenuatorLeftToLeft &&
			    lr == cdr.AttenuatorLeftToRight &&
			    rl == cdr.AttenuatorRightToLeft &&
			    rr == cdr.AttenuatorRightToRight)
				return;
			*((u32*)&cdr.AttenuatorLeftToLeft) = *((u32*)&cdr.AttenuatorLeftToLeftT);
			CDR_LOG_I("CD-XA Volume: %02x %02x | %02x %02x\n", ll, lr, rl, rr);
			SPU_setCDvol(ll, lr, rl, rr, psxRegs.cycle);
		}
		return;
	}

	// test: Viewpoint
	if ((rt & 0x80) && cdr.FifoOffset < cdr.FifoSize) {
		CDR_LOG("cdrom: FifoOffset(2) %d/%d\n", cdr.FifoOffset, cdr.FifoSize);
	}
	else if (rt & 0x80) {
		switch (cdr.Mode & (MODE_SIZE_2328|MODE_SIZE_2340)) {
			case MODE_SIZE_2328:
			case 0x00:
				cdr.FifoOffset = 12;
				cdr.FifoSize = 2048 + 12;
				break;

			case MODE_SIZE_2340:
			default:
				cdr.FifoOffset = 0;
				cdr.FifoSize = 2340;
				break;
		}
	}
	else if (!(rt & 0xc0))
		cdr.FifoOffset = DATA_SIZE; // fifo empty
}

void psxDma3(u32 madr, u32 bcr, u32 chcr) {
	u32 cdsize, max_words;
	int size;
	u8 *ptr;

	CDR_LOG("psxDma3() Log: *** DMA 3 *** %x addr = %x size = %x\n", chcr, madr, bcr);

	switch (chcr & 0x71000000) {
		case 0x11000000:
			madr &= ~3;
			ptr = getDmaRam(madr, &max_words);
			if (ptr == INVALID_PTR) {
				CDR_LOG_I("psxDma3() Log: *** DMA 3 *** NULL Pointer!\n");
				break;
			}

			cdsize = (((bcr - 1) & 0xffff) + 1) * 4;

			/*
			GS CDX: Enhancement CD crash
			- Setloc 0:0:0
			- CdlPlay
			- Spams DMA3 and gets buffer overrun
			*/
			size = DATA_SIZE - cdr.FifoOffset;
			if (size > cdsize)
				size = cdsize;
			if (size > max_words * 4)
				size = max_words * 4;
			if (size > 0)
			{
				memcpy(ptr, cdr.Transfer + cdr.FifoOffset, size);
				cdr.FifoOffset += size;
			}
			if (size < cdsize) {
				CDR_LOG_I("cdrom: dma3 %d/%d\n", size, cdsize);
				memset(ptr + size, cdr.Transfer[0x920], cdsize - size);
			}
			psxCpu->Clear(madr, cdsize / 4);

			set_event(PSXINT_CDRDMA, (cdsize / 4) * 24);

			HW_DMA3_CHCR &= SWAPu32(~0x10000000);
			if (chcr & 0x100) {
				HW_DMA3_MADR = SWAPu32(madr + cdsize);
				HW_DMA3_BCR &= SWAPu32(0xffff0000);
			}
			else {
				// halted
				psxRegs.cycle += (cdsize/4) * 24 - 20;
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

void cdrDmaInterrupt(void)
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
	cdr.FilterFile = 0;
	cdr.FilterChannel = 0;
	cdr.IrqMask = 0x1f;
	cdr.IrqStat = NoIntr;

	// BIOS player - default values
	cdr.AttenuatorLeftToLeft = 0x80;
	cdr.AttenuatorLeftToRight = 0x00;
	cdr.AttenuatorRightToLeft = 0x00;
	cdr.AttenuatorRightToRight = 0x80;

	softReset();
	getCdInfo();
}

int cdrFreeze(gzFile f, int Mode) {
	u32 tmp;
	u8 tmpp[4];

	if (Mode == 0 && !Config.Cdda)
		CDR_stop();

	cdr.freeze_ver = 0x63647202;
	gzfreeze(&cdr, sizeof(cdr));

	if (Mode == 1) {
		cdr.ParamP = cdr.ParamC;
		tmp = cdr.FifoOffset;
	}

	gzfreeze(&tmp, sizeof(tmp));

	if (Mode == 0) {
		u8 ll = 0, lr = 0, rl = 0, rr = 0;
		getCdInfo();

		cdr.FifoOffset = tmp < DATA_SIZE ? tmp : DATA_SIZE;
		cdr.FifoSize = (cdr.Mode & MODE_SIZE_2340) ? 2340 : 2048 + 12;
		if (cdr.SubqForwardSectors > SUBQ_FORWARD_SECTORS)
			cdr.SubqForwardSectors = SUBQ_FORWARD_SECTORS;

		// read right sub data
		tmpp[0] = btoi(cdr.Prev[0]);
		tmpp[1] = btoi(cdr.Prev[1]);
		tmpp[2] = btoi(cdr.Prev[2]);
		tmpp[3] = 0;
		cdr.Prev[0]++;
		ReadTrack(tmpp);

		if (cdr.Play) {
			if (cdr.freeze_ver < 0x63647202)
				memcpy(cdr.SetSectorPlay, cdr.SetSector, 3);

			Find_CurTrack(cdr.SetSectorPlay);
			if (!Config.Cdda)
				CDR_play(cdr.SetSectorPlay);
		}
		if (!cdr.Muted)
			ll = cdr.AttenuatorLeftToLeft, lr = cdr.AttenuatorLeftToLeft,
			rl = cdr.AttenuatorRightToLeft, rr = cdr.AttenuatorRightToRight;
		SPU_setCDvol(ll, lr, rl, rr, psxRegs.cycle);
	}

	return 0;
}

void LidInterrupt(void) {
	getCdInfo();
	cdrLidSeekInterrupt();
}
