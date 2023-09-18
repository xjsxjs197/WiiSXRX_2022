/***************************************************************************
 *   PCSX-Revolution - PlayStation Emulator for Nintendo Wii               *
 *   Copyright (C) 2009-2010  PCSX-Revolution Dev Team                     *
 *   <http://code.google.com/p/pcsx-revolution/>                           *
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
* GTE functions.
*/

#include "gte.h"
#include "gte_divider.h"
#include "psxmem.h"

#define VX(n) (regs->CP2D.p[n << 1].sw.h)
#define VY(n) (regs->CP2D.p[n << 1].sw.l)
#define VZ(n) (regs->CP2D.p[(n << 1) + 1].sw.h)

#define VX_MVMVA(n) (n < 3 ? regs->CP2D.p[n << 1].sw.h : regs->CP2D.p[9].sw.h)
#define VY_MVMVA(n) (n < 3 ? regs->CP2D.p[n << 1].sw.l : regs->CP2D.p[9].sw.l)
#define VZ_MVMVA(n) (n < 3 ? regs->CP2D.p[(n << 1) + 1].sw.h : regs->CP2D.p[10].sw.h)

#define MX11(n) (n < 3 ? regs->CP2C.p[(n << 3)].sw.h : 0)
#define MX12(n) (n < 3 ? regs->CP2C.p[(n << 3)].sw.l : 0)
#define MX13(n) (n < 3 ? regs->CP2C.p[(n << 3) + 1].sw.h : 0)
#define MX21(n) (n < 3 ? regs->CP2C.p[(n << 3) + 1].sw.l : 0)
#define MX22(n) (n < 3 ? regs->CP2C.p[(n << 3) + 2].sw.h : 0)
#define MX23(n) (n < 3 ? regs->CP2C.p[(n << 3) + 2].sw.l : 0)
#define MX31(n) (n < 3 ? regs->CP2C.p[(n << 3) + 3].sw.h : 0)
#define MX32(n) (n < 3 ? regs->CP2C.p[(n << 3) + 3].sw.l : 0)
#define MX33(n) (n < 3 ? regs->CP2C.p[(n << 3) + 4].sw.h : 0)
#define CV1(n) (n < 3 ? (s32)regs->CP2C.r[(n << 3) + 5] : 0)
#define CV2(n) (n < 3 ? (s32)regs->CP2C.r[(n << 3) + 6] : 0)
#define CV3(n) (n < 3 ? (s32)regs->CP2C.r[(n << 3) + 7] : 0)

#define fSX(n) ((regs->CP2D.p)[((n) + 12)].sw.l)
#define fSY(n) ((regs->CP2D.p)[((n) + 12)].sw.h)
#define fSZ(n) ((regs->CP2D.p)[((n) + 17)].w.l) /* (n == 0) => SZ1; */

//#define gteVXY0 (regs->CP2D.r[0])
#define gteVX0  (regs->CP2D.p[0].sw.h)
#define gteVY0  (regs->CP2D.p[0].sw.l)
#define gteVZ0  (regs->CP2D.p[1].sw.h)
#define gteVZ0L (regs->CP2D.p[1].sw.l)
//#define gteVXY1 (regs->CP2D.r[2])
#define gteVX1  (regs->CP2D.p[2].sw.h)
#define gteVY1  (regs->CP2D.p[2].sw.l)
#define gteVZ1  (regs->CP2D.p[3].sw.h)
#define gteVZ1L (regs->CP2D.p[3].sw.l)
//#define gteVXY2 (regs->CP2D.r[4])
#define gteVX2  (regs->CP2D.p[4].sw.h)
#define gteVY2  (regs->CP2D.p[4].sw.l)
#define gteVZ2  (regs->CP2D.p[5].sw.h)
#define gteVZ2L (regs->CP2D.p[5].sw.l)

#define gteRGB  (regs->CP2D.r[6])
#define gteR    (regs->CP2D.p[6].b.l)
#define gteG    (regs->CP2D.p[6].b.h)
#define gteB    (regs->CP2D.p[6].b.h2)
#define gteCODE (regs->CP2D.p[6].b.h3)
#define gteOTZ  (regs->CP2D.p[7].w.l)
#define gteIR0  (regs->CP2D.p[8].sw.l)

#define gteIR1  (regs->CP2D.p[9].sw.h)  // #define gteIR1  (regs->CP2D.p[9].sw.l)
#define gteIR2  (regs->CP2D.p[9].sw.l)  // #define gteIR2  (regs->CP2D.p[10].sw.l)
#define gteIR3  (regs->CP2D.p[10].sw.h) // #define gteIR3  (regs->CP2D.p[11].sw.l)
#define gteIR3L (regs->CP2D.p[10].sw.l)

#define gteSXY0 (regs->CP2D.r[12])
#define gteSX0  (regs->CP2D.p[12].sw.l)
#define gteSY0  (regs->CP2D.p[12].sw.h)
#define gteSXY1 (regs->CP2D.r[13])
#define gteSX1  (regs->CP2D.p[13].sw.l)
#define gteSY1  (regs->CP2D.p[13].sw.h)
#define gteSXY2 (regs->CP2D.r[14])
#define gteSX2  (regs->CP2D.p[14].sw.l)
#define gteSY2  (regs->CP2D.p[14].sw.h)
#define gteSXYP (regs->CP2D.r[15])
#define gteSXP  (regs->CP2D.p[15].sw.l)
#define gteSYP  (regs->CP2D.p[15].sw.h)
#define gteSZ0  (regs->CP2D.p[16].w.l)
#define gteSZ1  (regs->CP2D.p[17].w.l)
#define gteSZ2  (regs->CP2D.p[18].w.l)
#define gteSZ3  (regs->CP2D.p[19].w.l)
#define gteRGB0  (regs->CP2D.r[20])
#define gteR0    (regs->CP2D.p[20].b.l)
#define gteG0    (regs->CP2D.p[20].b.h)
#define gteB0    (regs->CP2D.p[20].b.h2)
#define gteCODE0 (regs->CP2D.p[20].b.h3)
#define gteRGB1  (regs->CP2D.r[21])
#define gteR1    (regs->CP2D.p[21].b.l)
#define gteG1    (regs->CP2D.p[21].b.h)
#define gteB1    (regs->CP2D.p[21].b.h2)
#define gteCODE1 (regs->CP2D.p[21].b.h3)
#define gteRGB2  (regs->CP2D.r[22])
#define gteR2    (regs->CP2D.p[22].b.l)
#define gteG2    (regs->CP2D.p[22].b.h)
#define gteB2    (regs->CP2D.p[22].b.h2)
#define gteCODE2 (regs->CP2D.p[22].b.h3)
#define gteRES1  (regs->CP2D.r[23])
#define gteMAC0  (((s32 *)regs->CP2D.r)[24])
#define gteMAC1  (((s32 *)regs->CP2D.r)[25])
#define gteMAC2  (((s32 *)regs->CP2D.r)[26])
#define gteMAC3  (((s32 *)regs->CP2D.r)[27])
#define gteIRGB  (regs->CP2D.r[28])
#define gteORGB  (regs->CP2D.r[29])
#define gteLZCS  (regs->CP2D.r[30])
#define gteLZCR  (regs->CP2D.r[31])

//#define gteR11R12 (((s32 *)regs->CP2C.r)[0])
//#define gteR22R23 (((s32 *)regs->CP2C.r)[2])
#define gteR11 (regs->CP2C.p[0].sw.h)
#define gteR12 (regs->CP2C.p[0].sw.l)
#define gteR13 (regs->CP2C.p[1].sw.h)
#define gteR21 (regs->CP2C.p[1].sw.l)
#define gteR22 (regs->CP2C.p[2].sw.h)
#define gteR23 (regs->CP2C.p[2].sw.l)
#define gteR31 (regs->CP2C.p[3].sw.h)
#define gteR32 (regs->CP2C.p[3].sw.l)
#define gteR33 (regs->CP2C.p[4].sw.h)

#define gteTRX (((s32 *)regs->CP2C.r)[5])
#define gteTRY (((s32 *)regs->CP2C.r)[6])
#define gteTRZ (((s32 *)regs->CP2C.r)[7])

#define gteL11 (regs->CP2C.p[8].sw.h)
#define gteL12 (regs->CP2C.p[8].sw.l)
#define gteL13 (regs->CP2C.p[9].sw.h)
#define gteL21 (regs->CP2C.p[9].sw.l)
#define gteL22 (regs->CP2C.p[10].sw.h)
#define gteL23 (regs->CP2C.p[10].sw.l)
#define gteL31 (regs->CP2C.p[11].sw.h)
#define gteL32 (regs->CP2C.p[11].sw.l)
#define gteL33 (regs->CP2C.p[12].sw.h)

#define gteRBK (((s32 *)regs->CP2C.r)[13])
#define gteGBK (((s32 *)regs->CP2C.r)[14])
#define gteBBK (((s32 *)regs->CP2C.r)[15])

#define gteLR1 (regs->CP2C.p[16].sw.h)
#define gteLR2 (regs->CP2C.p[16].sw.l)
#define gteLR3 (regs->CP2C.p[17].sw.h)
#define gteLG1 (regs->CP2C.p[17].sw.l)
#define gteLG2 (regs->CP2C.p[18].sw.h)
#define gteLG3 (regs->CP2C.p[18].sw.l)
#define gteLB1 (regs->CP2C.p[19].sw.h)
#define gteLB2 (regs->CP2C.p[19].sw.l)
#define gteLB3 (regs->CP2C.p[20].sw.h)

#define gteRFC (((s32 *)regs->CP2C.r)[21])
#define gteGFC (((s32 *)regs->CP2C.r)[22])
#define gteBFC (((s32 *)regs->CP2C.r)[23])
#define gteOFX (((s32 *)regs->CP2C.r)[24])
#define gteOFY (((s32 *)regs->CP2C.r)[25])

// senquack - gteH register is u16, not s16, and used in GTE that way.
//  HOWEVER when read back by CPU using CFC2, it will be incorrectly
//  sign-extended by bug in original hardware, according to Nocash docs
//  GTE section 'Screen Offset and Distance'. The emulator does this
//  sign extension when it is loaded to GTE by CTC2.
//#define gteH   (regs->CP2C.p[26].sw.l)
#define gteH   (regs->CP2C.p[26].w.l)

#define gteDQA (regs->CP2C.p[27].sw.l)
#define gteDQB (((s32 *)regs->CP2C.r)[28])
#define gteZSF3 (regs->CP2C.p[29].sw.l)
#define gteZSF4 (regs->CP2C.p[30].sw.l)
#define gteFLAG (regs->CP2C.r[31])

#define GTE_OP(op) ((op >> 20) & 31)
#define GTE_SF(op) ((op >> 19) & 1)
#define GTE_MX(op) ((op >> 17) & 3)
#define GTE_V(op) ((op >> 15) & 3)
#define GTE_CV(op) ((op >> 13) & 3)
#define GTE_CD(op) ((op >> 11) & 3) /* not used */
#define GTE_LM(op) ((op >> 10) & 1)
#define GTE_CT(op) ((op >> 6) & 15) /* not used */
#define GTE_FUNCT(op) (op & 63)

#define gteop (psxRegs.code & 0x1ffffff)


static inline s64 BOUNDS_(psxCP2Regs *regs, s64 n_value, s64 n_max, int n_maxflag, s64 n_min, int n_minflag) {
    if (n_value > n_max) {
        gteFLAG |= n_maxflag;
    } else if (n_value < n_min) {
        gteFLAG |= n_minflag;
    }
    return n_value;
}

static inline s32 LIM_(psxCP2Regs *regs, s32 value, s32 max, s32 min, u32 flag) {
    s32 ret = value;
    if (value > max) {
        gteFLAG |= flag;
        ret = max;
    } else if (value < min) {
        gteFLAG |= flag;
        ret = min;
    }
    return ret;
}

static inline u32 limE_(psxCP2Regs *regs, u32 result) {
    if (result > 0x1ffff) {
        gteFLAG |= (1 << 31) | (1 << 17);
        return 0x1ffff;
    }
    return result;
}
#define BOUNDS(n_value,n_max,n_maxflag,n_min,n_minflag) \
    BOUNDS_(regs,n_value,n_max,n_maxflag,n_min,n_minflag)
#define LIM(value,max,min,flag) \
    LIM_(regs,value,max,min,flag)
#define limE(result) \
    limE_(regs,result)

#define A1(a) BOUNDS((a), 0x7fffffff, (1 << 30), -(s64)0x80000000, (1 << 31) | (1 << 27))
#define A2(a) BOUNDS((a), 0x7fffffff, (1 << 29), -(s64)0x80000000, (1 << 31) | (1 << 26))
#define A3(a) BOUNDS((a), 0x7fffffff, (1 << 28), -(s64)0x80000000, (1 << 31) | (1 << 25))
#define limB1(a, l) LIM((a), 0x7fff, -0x8000 * !l, (1 << 31) | (1 << 24))
#define limB2(a, l) LIM((a), 0x7fff, -0x8000 * !l, (1 << 31) | (1 << 23))
#define limB3(a, l) LIM((a), 0x7fff, -0x8000 * !l, (1 << 22))
#define limC1(a) LIM((a), 0x00ff, 0x0000, (1 << 21))
#define limC2(a) LIM((a), 0x00ff, 0x0000, (1 << 20))
#define limC3(a) LIM((a), 0x00ff, 0x0000, (1 << 19))
#define limD(a) LIM((a), 0xffff, 0x0000, (1 << 31) | (1 << 18))

#define F(a) BOUNDS((a), 0x7fffffff, (1 << 31) | (1 << 16), -(s64)0x80000000, (1 << 31) | (1 << 15))
#define limG1(a) LIM((a), 0x3ff, -0x400, (1 << 31) | (1 << 14))
#define limG2(a) LIM((a), 0x3ff, -0x400, (1 << 31) | (1 << 13))
//Fix for Valkyrie Profile crash loading world map
// (PCSX Rearmed commit 7384197d8a5fd20a4d94f3517a6462f7fe86dd4c
//  'seems to work, unverified value')
//#define limH(a) LIM((a), 0xfff, 0x000, (1 << 12))
#define limH(a) LIM((a), 0x1000, 0x0000, (1 << 12))

#ifndef __arm__
#define A1U A1
#define A2U A2
#define A3U A3
#else
/* these are unlikely to be hit and usually waste cycles, don't want them on ARM */
#define A1U(x) (x)
#define A2U(x) (x)
#define A3U(x) (x)
#endif


//senquack - n param should be unsigned (will be 'gteH' reg which is u16)
#ifdef GTE_USE_NATIVE_DIVIDE
INLINE u32 DIVIDE_INT(u16 n, u16 d) {
    if (n < d * 2) {
        return ((u32)n << 16) / d;
    }
    return 0xffffffff;
}
#else
#include "gte_divider.h"
#endif // GTE_USE_NATIVE_DIVIDE

#ifndef FLAGLESS

static inline u32 MFC2(int reg) {
    psxCP2Regs *regs = &psxRegs.CP2;
    u32 value;
    switch (reg) {
        case 0:
        case 2:
        case 4:
            value = psxRegs.CP2D.r[reg];
            return ((value & 0xFFFF) << 16) | ((value >> 16) & 0xFFFF);

        case 1:
        case 3:
        case 5:
            return (s32)(psxRegs.CP2D.p[reg].sw.h);

        case 8:
            psxRegs.CP2D.r[reg] = (s32)psxRegs.CP2D.p[reg].sw.l;
            break;

        case 9: // gteIR1
            return (s32)(gteIR1);
        case 10: // gteIR2
            return (s32)(gteIR2);
        case 11: // gteIR3
            return (s32)(gteIR3);

        case 7:
        case 16:
        case 17:
        case 18:
        case 19:
            psxRegs.CP2D.r[reg] = (u32)psxRegs.CP2D.p[reg].w.l;
            break;

        case 15:
            psxRegs.CP2D.r[reg] = gteSXY2;
            break;

        case 28:
        case 29:
            psxRegs.CP2D.r[reg] = LIM(gteIR1 >> 7, 0x1f, 0, 0) |
                                    (LIM(gteIR2 >> 7, 0x1f, 0, 0) << 5) |
                                    (LIM(gteIR3 >> 7, 0x1f, 0, 0) << 10);
            break;
    }
    return psxRegs.CP2D.r[reg];
}

static inline void MTC2(u32 value, int reg) {
    psxCP2Regs *regs = &psxRegs.CP2;
    switch (reg) {
        case 0: // gteVX0, gteVY0
        case 2: // gteVX1, gteVY1
        case 4: // gteVX2, gteVY2
            psxRegs.CP2D.r[reg] = ((value & 0xFFFF) << 16) | ((value >> 16) & 0xFFFF);
            break;

        case 1: // gteVZ0
        case 3: // gteVZ1
        case 5: // gteVZ2
            psxRegs.CP2D.r[reg] = (value & 0xFFFF) << 16;
            break;

        case 9: // gteIR1
            gteIR1 = value & 0xFFFF;
            break;

        case 10: // gteIR2
            gteIR2 = value & 0xFFFF;
            break;

        case 11: // gteIR3
            psxRegs.CP2D.r[10] = (value & 0xFFFF) << 16;
            break;

        case 15:
            gteSXY0 = gteSXY1;
            gteSXY1 = gteSXY2;
            gteSXY2 = value;
            gteSXYP = value;
            break;

        case 28:
            gteIRGB = value;
            gteIR1 = (value & 0x1f) << 7;
            gteIR2 = (value & 0x3e0) << 2;
            gteIR3 = (value & 0x7c00) >> 3;
            break;

        case 30:
            {
                int a;
                gteLZCS = value;

                a = gteLZCS;
                if (a > 0) {
                    int i;
                    for (i = 31; (a & (1 << i)) == 0 && i >= 0; i--);
                    gteLZCR = 31 - i;
                } else if (a < 0) {
                    int i;
                    a ^= 0xffffffff;
                    for (i = 31; (a & (1 << i)) == 0 && i >= 0; i--);
                    gteLZCR = 31 - i;
                } else {
                    gteLZCR = 32;
                }
            }
            break;

        case 31:
            return;

        default:
            psxRegs.CP2D.r[reg] = value;
    }
}

static inline void CTC2(u32 value, int reg) {
    switch (reg) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 8:
        case 9:
        case 10:
        case 11:
        case 16:
        case 17:
        case 18:
        case 19:
            value = ((value & 0xFFFF) << 16) | ((value >> 16) & 0xFFFF);
            break;

        case 4:
        case 12:
        case 20:
            value = (value & 0xFFFF) << 16;
            break;
        case 26:
        case 27:
        case 29:
        case 30:
            value = (s32)(s16)value;
            break;

        case 31:
            value = value & 0x7ffff000;
            if (value & 0x7f87e000) value |= 0x80000000;
            break;
    }

    psxRegs.CP2C.r[reg] = value;
}

void gteMFC2() {
    if (!_Rt_) return;
    psxRegs.GPR.r[_Rt_] = MFC2(_Rd_);
}

void gteCFC2() {
    if (!_Rt_) return;
    u32 value = psxRegs.CP2C.r[_Rd_];
    switch (_Rd_) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 8:
        case 9:
        case 10:
        case 11:
        case 16:
        case 17:
        case 18:
        case 19:
            value = ((value & 0xFFFF) << 16) | ((value >> 16) & 0xFFFF);
            break;

        case 4:
        case 12:
        case 20:
            //value = (value >> 16) & 0xFFFF;
            value = ((s32)value) >> 16;
            break;
    }
    psxRegs.GPR.r[_Rt_] = value;
}

void gteMTC2() {
    MTC2(psxRegs.GPR.r[_Rt_], _Rd_);
}

void gteCTC2() {
    CTC2(psxRegs.GPR.r[_Rt_], _Rd_);
}

#define _oB_ (psxRegs.GPR.r[_Rs_] + _Imm_)

void gteLWC2() {
    MTC2(psxMemRead32(_oB_), _Rt_);
}

void gteSWC2() {
    psxMemWrite32(_oB_, MFC2(_Rt_));
}

#endif // FLAGLESS

#define RESET_MAC() \
    gteMAC1 = tmpMAC1; \
    gteMAC2 = tmpMAC2; \
    gteMAC3 = tmpMAC3;

#define RTPS_FOOTER() \
    tmp = (s64)gteDQB + ((s64)gteDQA * quotient); \
    gteMAC0 = F(tmp); \
    gteIR0 = limH(tmp >> 12); \
     \
    RESET_MAC();

extern void guVecMultiplySR(register s16 *mt, register s16 *vector, register s32 *ret);

void gteRTPS(psxCP2Regs *regs) {
    int quotient;
    s64 tmp;

#ifdef GTE_LOG
    GTE_LOG("GTE RTPS\n");
#endif
    gteFLAG = 0;
    s32 tmpMAC[3];
    s32 tmpMAC1, tmpMAC2, tmpMAC3;

    //tmpMAC1 = A1(((s64)gteTRX) + ((((s32)gteR11 * (s32)gteVX0) + ((s32)gteR12 * (s32)gteVY0) + ((s32)gteR13 * (s32)gteVZ0)) >> 12));
    //tmpMAC2 = A2(((s64)gteTRY) + ((((s32)gteR21 * (s32)gteVX0) + ((s32)gteR22 * (s32)gteVY0) + ((s32)gteR23 * (s32)gteVZ0)) >> 12));
    //gteMAC3 = A3(((s64)gteTRZ) + ((((s32)gteR31 * (s32)gteVX0) + ((s32)gteR32 * (s32)gteVY0) + ((s32)gteR33 * (s32)gteVZ0)) >> 12));
    guVecMultiplySR(&gteR11, &gteVX0, tmpMAC);
    tmpMAC1 = A1(((s64)gteTRX) + (tmpMAC[0] >> 12));
    tmpMAC2 = A2(((s64)gteTRY) + (tmpMAC[1] >> 12));
    tmpMAC3 = A3(((s64)gteTRZ) + (tmpMAC[2] >> 12));

    gteIR1 = limB1(tmpMAC1, 0);
    gteIR2 = limB2(tmpMAC2, 0);
    gteIR3 = limB3(tmpMAC3, 0);
    gteSZ0 = gteSZ1;
    gteSZ1 = gteSZ2;
    gteSZ2 = gteSZ3;
    gteSZ3 = limD(tmpMAC3);
    quotient = limE(DIVIDE_INT(gteH, gteSZ3));
    gteSXY0 = gteSXY1;
    gteSXY1 = gteSXY2;
    gteSX2 = limG1(F((s64)gteOFX + ((s64)gteIR1 * quotient)) >> 16);
    gteSY2 = limG2(F((s64)gteOFY + ((s64)gteIR2 * quotient)) >> 16);

    tmp = (s64)gteDQB + ((s64)gteDQA * quotient);
    gteMAC0 = F(tmp);
    gteIR0 = limH(tmp >> 12);

    RESET_MAC();
}

#define RTPT_ITEM(v) { \
    /*vx = VX(v);*/ \
    /*vy = VY(v);*/ \
    /*vz = VZ(v);*/ \
    /*tmpMAC1 = A1(((s64)gteTRX) + ((((s32)gteR11 * (s32)vx) + ((s32)gteR12 * (s32)vy) + ((s32)gteR13 * (s32)vz)) >> 12));*/ \
    /*tmpMAC2 = A2(((s64)gteTRY) + ((((s32)gteR21 * (s32)vx) + ((s32)gteR22 * (s32)vy) + ((s32)gteR23 * (s32)vz)) >> 12));*/ \
    /*tmpMAC3 = A3(((s64)gteTRZ) + ((((s32)gteR31 * (s32)vx) + ((s32)gteR32 * (s32)vy) + ((s32)gteR33 * (s32)vz)) >> 12));*/ \
    guVecMultiplySR(&gteR11, &VX(v), tmpMAC); \
    tmpMAC1 = A1(((s64)gteTRX) + (tmpMAC[0] >> 12)); \
    tmpMAC2 = A2(((s64)gteTRY) + (tmpMAC[1] >> 12)); \
    tmpMAC3 = A3(((s64)gteTRZ) + (tmpMAC[2] >> 12)); \
    tmpIR1 = limB1(tmpMAC1, 0); \
    tmpIR2 = limB2(tmpMAC2, 0); \
    tmpIR3 = limB3(tmpMAC3, 0); \
    fSZ(v) = limD(tmpMAC3); \
    quotient = limE(DIVIDE_INT(gteH, fSZ(v))); \
    fSX(v) = limG1(F((s64)gteOFX + ((s64)tmpIR1 * quotient)) >> 16); \
    fSY(v) = limG2(F((s64)gteOFY + ((s64)tmpIR2 * quotient)) >> 16); \
}

void gteRTPT(psxCP2Regs *regs) {
    int quotient;
    int v;
    s32 vx, vy, vz;
    s64 tmp;
    s32 tmpMAC[3];
    s32 tmpMAC1, tmpMAC2, tmpMAC3;
    s32 tmpIR1, tmpIR2, tmpIR3;

    gteFLAG = 0;

    gteSZ0 = gteSZ3;
    RTPT_ITEM(0);
    RTPT_ITEM(1);
    RTPT_ITEM(2);

    gteIR1 = tmpIR1;
    gteIR2 = tmpIR2;
    gteIR3 = tmpIR3;

    RTPS_FOOTER();
}

#define SET_MVMVA_IR(lm) \
    if (lm) { \
        gteIR1 = limB1(gteMAC1, 1); \
        gteIR2 = limB2(gteMAC2, 1); \
        gteIR3 = limB3(gteMAC3, 1); \
    } \
    else { \
        gteIR1 = limB1(gteMAC1, 0); \
        gteIR2 = limB2(gteMAC2, 0); \
        gteIR3 = limB3(gteMAC3, 0); \
    }

#define gte_C11 gteLR1
#define gte_C12 gteLR2
#define gte_C13 gteLR3
#define gte_C21 gteLG1
#define gte_C22 gteLG2
#define gte_C23 gteLG3
#define gte_C31 gteLB1
#define gte_C32 gteLB2
#define gte_C33 gteLB3

#define _MVMVA_FUNC(_v0, _v1, _v2, mx) { \
    /*SSX = (_v0) * mx##11 + (_v1) * mx##12 + (_v2) * mx##13;*/ \
    /*SSY = (_v0) * mx##21 + (_v1) * mx##22 + (_v2) * mx##23;*/ \
    /*SSZ = (_v0) * mx##31 + (_v1) * mx##32 + (_v2) * mx##33;*/ \
    guVecMultiplySR(&(mx##11), &(_v0), tmpMAC); \
    SSX = tmpMAC[0]; \
    SSY = tmpMAC[1]; \
    SSZ = tmpMAC[2]; \
}

void gteMVMVA(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    s32 tmpMAC[3];
    gteFLAG = 0;

    s32 checkCode = psxRegs.code >> 12;

    switch (checkCode & 0x78) {
        case 0x00: // V0 * R
            _MVMVA_FUNC(gteVX0, gteVY0, gteVZ0, gteR); break;
        case 0x08: // V1 * R
            _MVMVA_FUNC(gteVX1, gteVY1, gteVZ1, gteR); break;
        case 0x10: // V2 * R
            _MVMVA_FUNC(gteVX2, gteVY2, gteVZ2, gteR); break;
        case 0x18: // IR * R
            _MVMVA_FUNC(gteIR1, gteIR2, gteIR3, gteR); break;
        case 0x20: // V0 * L
            _MVMVA_FUNC(gteVX0, gteVY0, gteVZ0, gteL); break;
        case 0x28: // V1 * L
            _MVMVA_FUNC(gteVX1, gteVY1, gteVZ1, gteL); break;
        case 0x30: // V2 * L
            _MVMVA_FUNC(gteVX2, gteVY2, gteVZ2, gteL); break;
        case 0x38: // IR * L
            _MVMVA_FUNC(gteIR1, gteIR2, gteIR3, gteL); break;
        case 0x40: // V0 * C
            _MVMVA_FUNC(gteVX0, gteVY0, gteVZ0, gte_C); break;
        case 0x48: // V1 * C
            _MVMVA_FUNC(gteVX1, gteVY1, gteVZ1, gte_C); break;
        case 0x50: // V2 * C
            _MVMVA_FUNC(gteVX2, gteVY2, gteVZ2, gte_C); break;
        case 0x58: // IR * C
            _MVMVA_FUNC(gteIR1, gteIR2, gteIR3, gte_C); break;
        default:
            SSX = SSY = SSZ = 0;
    }

    switch (checkCode & 0x6) {
        case 0x0: // Add TR
            SSX += (s64)gteTRX << 12;
            SSY += (s64)gteTRY << 12;
            SSZ += (s64)gteTRZ << 12;
            break;
        case 0x2: // Add BK
            SSX += (s64)gteRBK << 12;
            SSY += (s64)gteGBK << 12;
            SSZ += (s64)gteBBK << 12;
            break;
        case 0x4: // Add FC
            SSX += (s64)gteRFC << 12;
            SSY += (s64)gteGFC << 12;
            SSZ += (s64)gteBFC << 12;
            break;
    }

    if (checkCode & 0x80) {
        SSX >>= 12; SSY >>= 12; SSZ >>= 12;
    }
    gteMAC1 = A1(SSX);
    gteMAC2 = A2(SSY);
    gteMAC3 = A3(SSZ);

    SET_MVMVA_IR(psxRegs.code & 0x400);
}

void gteNCLIP(psxCP2Regs *regs) {
#ifdef GTE_LOG
    GTE_LOG("GTE NCLIP\n");
#endif
    gteFLAG = 0;

    gteMAC0 = F((s64)gteSX0 * (gteSY1 - gteSY2) +
                gteSX1 * (gteSY2 - gteSY0) +
                gteSX2 * (gteSY0 - gteSY1));
}

void gteAVSZ3(psxCP2Regs *regs) {
#ifdef GTE_LOG
    GTE_LOG("GTE AVSZ3\n");
#endif
    gteFLAG = 0;

    gteMAC0 = F((s64)gteZSF3 * (s32)((u32)gteSZ1 + (u32)gteSZ2 + (u32)gteSZ3));
    gteOTZ = limD(gteMAC0 >> 12);
}

void gteAVSZ4(psxCP2Regs *regs) {
#ifdef GTE_LOG
    GTE_LOG("GTE AVSZ4\n");
#endif
    gteFLAG = 0;

    gteMAC0 = F((s64)gteZSF4 * (s32)((u32)gteSZ0 + (u32)gteSZ1 + (u32)gteSZ2 + (u32)gteSZ3));
    gteOTZ = limD(gteMAC0 >> 12);
}

#define SQL_FOOTER(lm) \
    gteIR1 = limB1(tmpMAC1, lm); \
    gteIR2 = limB2(tmpMAC2, lm); \
    gteIR3 = limB3(tmpMAC3, lm); \
     \
    RESET_MAC();

void gteSQR(psxCP2Regs *regs) {
    s32 tmpMAC1, tmpMAC2, tmpMAC3;
    gteFLAG = 0;

    if (psxRegs.code & 0x80000) {
        tmpMAC1 = ((s32)gteIR1 * (s32)gteIR1) >> 12;
        tmpMAC2 = ((s32)gteIR2 * (s32)gteIR2) >> 12;
        tmpMAC3 = ((s32)gteIR3 * (s32)gteIR3) >> 12;
    } else {
        tmpMAC1 = ((s32)gteIR1 * (s32)gteIR1);
        tmpMAC2 = ((s32)gteIR2 * (s32)gteIR2);
        tmpMAC3 = ((s32)gteIR3 * (s32)gteIR3);
    }

    if (psxRegs.code & 0x400)
    {
        SQL_FOOTER(1);
    }
    else
    {
        SQL_FOOTER(0);
    }
}

#define NCCS(v) \
    /*vx = VX(v);*/ \
    /*vy = VY(v);*/ \
    /*vz = VZ(v);*/ \
    /*tmpMAC1 = ((s32)gteL11 * vx + (s32)gteL12 * vy + (s32)gteL13 * vz) >> 12;*/ \
    /*tmpMAC2 = ((s32)gteL21 * vx + (s32)gteL22 * vy + (s32)gteL23 * vz) >> 12;*/ \
    /*tmpMAC3 = ((s32)gteL31 * vx + (s32)gteL32 * vy + (s32)gteL33 * vz) >> 12;*/ \
    guVecMultiplySR(&gteL11, &VX(v), tmpMAC); \
    tmpMAC1 = (tmpMAC[0] >> 12); \
    tmpMAC2 = (tmpMAC[1] >> 12); \
    tmpMAC3 = (tmpMAC[2] >> 12); \
    gteIR1 = limB1(tmpMAC1, 1); \
    gteIR2 = limB2(tmpMAC2, 1); \
    gteIR3 = limB3(tmpMAC3, 1); \
    /*tmpMAC1 = A1(((s64)gteRBK) + (((s32)gteLR1 * (s32)gteIR1 + (s32)gteLR2 * gteIR2 + (s32)gteLR3 * gteIR3) >> 12));*/ \
    /*tmpMAC2 = A2(((s64)gteGBK) + (((s32)gteLG1 * (s32)gteIR1 + (s32)gteLG2 * gteIR2 + (s32)gteLG3 * gteIR3) >> 12));*/ \
    /*tmpMAC3 = A3(((s64)gteBBK) + (((s32)gteLB1 * (s32)gteIR1 + (s32)gteLB2 * gteIR2 + (s32)gteLB3 * gteIR3) >> 12));*/ \
    guVecMultiplySR(&gteLR1, &gteIR1, tmpMAC); \
    tmpMAC1 = A1(((s64)gteRBK) + (tmpMAC[0] >> 12)); \
    tmpMAC2 = A2(((s64)gteGBK) + (tmpMAC[1] >> 12)); \
    tmpMAC3 = A3(((s64)gteBBK) + (tmpMAC[2] >> 12)); \
    gteIR1 = limB1(tmpMAC1, 1); \
    gteIR2 = limB2(tmpMAC2, 1); \
    gteIR3 = limB3(tmpMAC3, 1); \
    tmpMAC1 = ((s32)gteR * gteIR1) >> 8; \
    tmpMAC2 = ((s32)gteG * gteIR2) >> 8; \
    tmpMAC3 = ((s32)gteB * gteIR3) >> 8; \
     \
    gteRGB0 = gteRGB1; \
    gteRGB1 = gteRGB2; \
    gteCODE2 = gteCODE; \
    gteR2 = limC1(tmpMAC1 >> 4); \
    gteG2 = limC2(tmpMAC2 >> 4); \
    gteB2 = limC3(tmpMAC3 >> 4);

void gteNCCS(psxCP2Regs *regs) {
    s32 vx, vy, vz;
    s32 tmpMAC[3];
    s32 tmpMAC1, tmpMAC2, tmpMAC3;
    gteFLAG = 0;

    NCCS(0);

    gteIR1 = tmpMAC1;
    gteIR2 = tmpMAC2;
    gteIR3 = tmpMAC3;

    RESET_MAC();
}

void gteNCCT(psxCP2Regs *regs) {
    s32 vx, vy, vz;
    s32 tmpMAC[3];
    s32 tmpMAC1, tmpMAC2, tmpMAC3;
    gteFLAG = 0;

    NCCS(0);
    NCCS(1);
    NCCS(2);

    gteIR1 = tmpMAC1;
    gteIR2 = tmpMAC2;
    gteIR3 = tmpMAC3;

    RESET_MAC();
}

#define NCDS(v) \
    /*vx = VX(v);*/ \
    /*vy = VY(v);*/ \
    /*vz = VZ(v);*/ \
    /*tmpMAC1 = ((s32)gteL11 * vx + (s32)gteL12 * vy + (s32)gteL13 * vz) >> 12;*/ \
    /*tmpMAC2 = ((s32)gteL21 * vx + (s32)gteL22 * vy + (s32)gteL23 * vz) >> 12;*/ \
    /*tmpMAC3 = ((s32)gteL31 * vx + (s32)gteL32 * vy + (s32)gteL33 * vz) >> 12;*/ \
    guVecMultiplySR(&gteL11, &VX(v), tmpMAC); \
    tmpMAC1 = (tmpMAC[0] >> 12); \
    tmpMAC2 = (tmpMAC[1] >> 12); \
    tmpMAC3 = (tmpMAC[2] >> 12); \
    gteIR1 = limB1(tmpMAC1, 1); \
    gteIR2 = limB2(tmpMAC2, 1); \
    gteIR3 = limB3(tmpMAC3, 1); \
    /*tmpMAC1 = A1(((s64)gteRBK) + (((s32)gteLR1 * gteIR1 + (s32)gteLR2 * gteIR2 + (s32)gteLR3 * gteIR3) >> 12));*/ \
    /*tmpMAC2 = A2(((s64)gteGBK) + (((s32)gteLG1 * gteIR1 + (s32)gteLG2 * gteIR2 + (s32)gteLG3 * gteIR3) >> 12));*/ \
    /*tmpMAC3 = A3(((s64)gteBBK) + (((s32)gteLB1 * gteIR1 + (s32)gteLB2 * gteIR2 + (s32)gteLB3 * gteIR3) >> 12));*/ \
    guVecMultiplySR(&gteLR1, &gteIR1, tmpMAC); \
    tmpMAC1 = A1(((s64)gteRBK) + (tmpMAC[0] >> 12)); \
    tmpMAC2 = A2(((s64)gteGBK) + (tmpMAC[1] >> 12)); \
    tmpMAC3 = A3(((s64)gteBBK) + (tmpMAC[2] >> 12)); \
    gteIR1 = limB1(tmpMAC1, 1); \
    gteIR2 = limB2(tmpMAC2, 1); \
    gteIR3 = limB3(tmpMAC3, 1); \
    tmpMAC1 = (((gteR << 4) * gteIR1) + (gteIR0 * limB1(A1U((s64)gteRFC - ((gteR * gteIR1) >> 8)), 0))) >> 12; \
    tmpMAC2 = (((gteG << 4) * gteIR2) + (gteIR0 * limB2(A2U((s64)gteGFC - ((gteG * gteIR2) >> 8)), 0))) >> 12; \
    tmpMAC3 = (((gteB << 4) * gteIR3) + (gteIR0 * limB3(A3U((s64)gteBFC - ((gteB * gteIR3) >> 8)), 0))) >> 12; \
     \
    gteRGB0 = gteRGB1; \
    gteRGB1 = gteRGB2; \
    gteCODE2 = gteCODE; \
    gteR2 = limC1(tmpMAC1 >> 4); \
    gteG2 = limC2(tmpMAC2 >> 4); \
    gteB2 = limC3(tmpMAC3 >> 4);

void gteNCDS(psxCP2Regs *regs) {
    s32 vx, vy, vz;
    s32 tmpMAC[3];
    s32 tmpMAC1, tmpMAC2, tmpMAC3;
    gteFLAG = 0;

    NCDS(0);

    gteIR1 = limB1(tmpMAC1, 1);
    gteIR2 = limB2(tmpMAC2, 1);
    gteIR3 = limB3(tmpMAC3, 1);

    RESET_MAC();
}

void gteNCDT(psxCP2Regs *regs) {
    s32 vx, vy, vz;
    s32 tmpMAC[3];
    s32 tmpMAC1, tmpMAC2, tmpMAC3;
    gteFLAG = 0;

    NCDS(0);
    NCDS(1);
    NCDS(2);

    gteIR1 = limB1(tmpMAC1, 1);
    gteIR2 = limB2(tmpMAC2, 1);
    gteIR3 = limB3(tmpMAC3, 1);

    RESET_MAC();
}

#define OP_FOOTER(lm) \
    gteIR1 = limB1(tmpMAC1, lm); \
    gteIR2 = limB2(tmpMAC2, lm); \
    gteIR3 = limB3(tmpMAC3, lm); \
     \
    RESET_MAC();

void gteOP(psxCP2Regs *regs) {
    s32 tmpMAC1, tmpMAC2, tmpMAC3;
    gteFLAG = 0;

    if (psxRegs.code & 0x80000) {
        tmpMAC1 = (((s32)gteR22 * gteIR3) - ((s32)gteR33 * gteIR2)) >> 12;
        tmpMAC2 = (((s32)gteR33 * gteIR1) - ((s32)gteR11 * gteIR3)) >> 12;
        tmpMAC3 = (((s32)gteR11 * gteIR2) - ((s32)gteR22 * gteIR1)) >> 12;
    } else {
        tmpMAC1 = (((s32)gteR22 * gteIR3) - ((s32)gteR33 * gteIR2));
        tmpMAC2 = (((s32)gteR33 * gteIR1) - ((s32)gteR11 * gteIR3));
        tmpMAC3 = (((s32)gteR11 * gteIR2) - ((s32)gteR22 * gteIR1));
    }

    if (psxRegs.code & 0x400)
    {
        OP_FOOTER(1);
    }
    else
    {
        OP_FOOTER(0);
    }
}

#define DCPL(lm) \
    s32 RIR1 = ((s32)gteR * gteIR1) >> 8; \
    s32 GIR2 = ((s32)gteG * gteIR2) >> 8; \
    s32 BIR3 = ((s32)gteB * gteIR3) >> 8; \
     \
    gteFLAG = 0; \
     \
    tmpMAC1 = RIR1 + ((gteIR0 * limB1(A1U((s64)gteRFC - RIR1), 0)) >> 12); \
    tmpMAC2 = GIR2 + ((gteIR0 * limB1(A2U((s64)gteGFC - GIR2), 0)) >> 12); \
    tmpMAC3 = BIR3 + ((gteIR0 * limB1(A3U((s64)gteBFC - BIR3), 0)) >> 12); \
     \
    gteIR1 = limB1(tmpMAC1, lm); \
    gteIR2 = limB2(tmpMAC2, lm); \
    gteIR3 = limB3(tmpMAC3, lm); \
     \
    gteRGB0 = gteRGB1; \
    gteRGB1 = gteRGB2; \
    gteCODE2 = gteCODE; \
    gteR2 = limC1(tmpMAC1 >> 4); \
    gteG2 = limC2(tmpMAC2 >> 4); \
    gteB2 = limC3(tmpMAC3 >> 4); \
     \
    RESET_MAC();

void gteDCPL(psxCP2Regs *regs) {
    s32 tmpMAC1, tmpMAC2, tmpMAC3;

    if (psxRegs.code & 0x400)
    {
        DCPL(1);
    }
    else
    {
        DCPL(0);
    }
}

void gteGPF(psxCP2Regs *regs) {
    s32 tmpMAC1, tmpMAC2, tmpMAC3;
    gteFLAG = 0;

    if (psxRegs.code & 0x80000) {
        tmpMAC1 = ((s32)gteIR0 * (s32)gteIR1) >> 12;
        tmpMAC2 = ((s32)gteIR0 * (s32)gteIR2) >> 12;
        tmpMAC3 = ((s32)gteIR0 * (s32)gteIR3) >> 12;
    } else {
        tmpMAC1 = (s32)gteIR0 * (s32)gteIR1;
        tmpMAC2 = (s32)gteIR0 * (s32)gteIR2;
        tmpMAC3 = (s32)gteIR0 * (s32)gteIR3;
    }

    gteIR1 = limB1(tmpMAC1, 0);
    gteIR2 = limB2(tmpMAC2, 0);
    gteIR3 = limB3(tmpMAC3, 0);

    gteRGB0 = gteRGB1;
    gteRGB1 = gteRGB2;
    gteCODE2 = gteCODE;
    gteR2 = limC1(tmpMAC1 >> 4);
    gteG2 = limC2(tmpMAC2 >> 4);
    gteB2 = limC3(tmpMAC3 >> 4);

    RESET_MAC();
}

void gteGPL(psxCP2Regs *regs) {
    s32 tmpMAC1, tmpMAC2, tmpMAC3;
    gteFLAG = 0;

    if (psxRegs.code & 0x80000) {
        tmpMAC1 = A1(((s64)gteMAC1) + (((s32)gteIR0 * (s32)gteIR1) >> 12));
        tmpMAC2 = A2(((s64)gteMAC2) + (((s32)gteIR0 * (s32)gteIR2) >> 12));
        tmpMAC3 = A3(((s64)gteMAC3) + (((s32)gteIR0 * (s32)gteIR3) >> 12));
    } else {
        tmpMAC1 = A1((((s64)gteMAC1) + ((s32)gteIR0 * (s32)gteIR1)));
        tmpMAC2 = A2((((s64)gteMAC2) + ((s32)gteIR0 * (s32)gteIR2)));
        tmpMAC3 = A3((((s64)gteMAC3) + ((s32)gteIR0 * (s32)gteIR3)));
    }

    gteIR1 = limB1(tmpMAC1, 0);
    gteIR2 = limB2(tmpMAC2, 0);
    gteIR3 = limB3(tmpMAC3, 0);

    gteRGB0 = gteRGB1;
    gteRGB1 = gteRGB2;
    gteCODE2 = gteCODE;
    gteR2 = limC1(tmpMAC1 >> 4);
    gteG2 = limC2(tmpMAC2 >> 4);
    gteB2 = limC3(tmpMAC3 >> 4);

    RESET_MAC();
}

void gteDPCS(psxCP2Regs *regs) {
    int shift = 12 * GTE_SF(gteop);

#ifdef GTE_LOG
    GTE_LOG("GTE DPCS\n");
#endif
    gteFLAG = 0;

    gteMAC1 = ((gteR << 16) + (gteIR0 * limB1(A1U(((s64)gteRFC - (gteR << 4)) << (12 - shift)), 0))) >> 12;
    gteMAC2 = ((gteG << 16) + (gteIR0 * limB2(A2U(((s64)gteGFC - (gteG << 4)) << (12 - shift)), 0))) >> 12;
    gteMAC3 = ((gteB << 16) + (gteIR0 * limB3(A3U(((s64)gteBFC - (gteB << 4)) << (12 - shift)), 0))) >> 12;

    gteIR1 = limB1(gteMAC1, 0);
    gteIR2 = limB2(gteMAC2, 0);
    gteIR3 = limB3(gteMAC3, 0);
    gteRGB0 = gteRGB1;
    gteRGB1 = gteRGB2;
    gteCODE2 = gteCODE;
    gteR2 = limC1(gteMAC1 >> 4);
    gteG2 = limC2(gteMAC2 >> 4);
    gteB2 = limC3(gteMAC3 >> 4);
}

#define DPCT_ITEM(v) { \
    tmpMAC1 = ((gteR0 << 16) + (gteIR0 * limB1(A1U((s64)gteRFC - (gteR0 << 4)), 0))) >> 12; \
    tmpMAC2 = ((gteG0 << 16) + (gteIR0 * limB1(A2U((s64)gteGFC - (gteG0 << 4)), 0))) >> 12; \
    tmpMAC3 = ((gteB0 << 16) + (gteIR0 * limB1(A3U((s64)gteBFC - (gteB0 << 4)), 0))) >> 12; \
     \
    gteRGB0 = gteRGB1; \
    gteRGB1 = gteRGB2; \
    gteCODE2 = gteCODE; \
    gteR2 = limC1(tmpMAC1 >> 4); \
    gteG2 = limC2(tmpMAC2 >> 4); \
    gteB2 = limC3(tmpMAC3 >> 4); \
}

void gteDPCT(psxCP2Regs *regs) {
    s32 tmpMAC1, tmpMAC2, tmpMAC3;
    gteFLAG = 0;

    DPCT_ITEM(0);
    DPCT_ITEM(1);
    DPCT_ITEM(2);

    gteIR1 = limB1(tmpMAC1, 0);
    gteIR2 = limB2(tmpMAC2, 0);
    gteIR3 = limB3(tmpMAC3, 0);

    RESET_MAC();
}

#define NCS(v) \
    /*vx = VX(v);*/ \
    /*vy = VY(v);*/ \
    /*vz = VZ(v);*/ \
    /*tmpMAC1 = (((s32)gteL11 * (s32)vx) + ((s32)gteL12 * (s32)vy) + ((s32)gteL13 * (s32)vz)) >> 12;*/ \
    /*tmpMAC2 = (((s32)gteL21 * (s32)vx) + ((s32)gteL22 * (s32)vy) + ((s32)gteL23 * (s32)vz)) >> 12;*/ \
    /*tmpMAC3 = (((s32)gteL31 * (s32)vx) + ((s32)gteL32 * (s32)vy) + ((s32)gteL33 * (s32)vz)) >> 12;*/ \
    guVecMultiplySR(&gteL11, &VX(v), tmpMAC); \
    tmpMAC1 = (tmpMAC[0] >> 12); \
    tmpMAC2 = (tmpMAC[1] >> 12); \
    tmpMAC3 = (tmpMAC[2] >> 12); \
    gteIR1 = limB1(tmpMAC1, 1); \
    gteIR2 = limB2(tmpMAC2, 1); \
    gteIR3 = limB3(tmpMAC3, 1); \
    /*tmpMAC1 = A1(((s64)gteRBK) + (((s32)gteLR1 * (s32)gteIR1 + (s32)gteLR2 * (s32)gteIR2 + (s32)gteLR3 * (s32)gteIR3) >> 12));*/ \
    /*tmpMAC2 = A2(((s64)gteGBK) + (((s32)gteLG1 * (s32)gteIR1 + (s32)gteLG2 * (s32)gteIR2 + (s32)gteLG3 * (s32)gteIR3) >> 12));*/ \
    /*tmpMAC3 = A3(((s64)gteBBK) + (((s32)gteLB1 * (s32)gteIR1 + (s32)gteLB2 * (s32)gteIR2 + (s32)gteLB3 * (s32)gteIR3) >> 12));*/ \
    guVecMultiplySR(&gteLR1, &gteIR1, tmpMAC); \
    tmpMAC1 = A1(((s64)gteRBK) + (tmpMAC[0] >> 12)); \
    tmpMAC2 = A2(((s64)gteGBK) + (tmpMAC[1] >> 12)); \
    tmpMAC3 = A3(((s64)gteBBK) + (tmpMAC[2] >> 12)); \
    gteRGB0 = gteRGB1; \
    gteRGB1 = gteRGB2; \
    gteCODE2 = gteCODE; \
    gteR2 = limC1(tmpMAC1 >> 4); \
    gteG2 = limC2(tmpMAC2 >> 4); \
    gteB2 = limC3(tmpMAC3 >> 4);

void gteNCS(psxCP2Regs *regs) {
    s32 vx, vy, vz;
    s32 tmpMAC[3];
    s32 tmpMAC1, tmpMAC2, tmpMAC3;
    gteFLAG = 0;

    NCS(0);

    gteIR1 = limB1(tmpMAC1, 1);
    gteIR2 = limB2(tmpMAC2, 1);
    gteIR3 = limB3(tmpMAC3, 1);

    RESET_MAC();
}

void gteNCT(psxCP2Regs *regs) {
    s32 vx, vy, vz;
    s32 tmpMAC[3];
    s32 tmpMAC1, tmpMAC2, tmpMAC3;
    gteFLAG = 0;

    NCS(0);
    NCS(1);
    NCS(2);

    gteIR1 = limB1(tmpMAC1, 1);
    gteIR2 = limB2(tmpMAC2, 1);
    gteIR3 = limB3(tmpMAC3, 1);

    RESET_MAC();
}

void gteCC(psxCP2Regs *regs) {
    s32 tmpMAC[3];
    s32 tmpMAC1, tmpMAC2, tmpMAC3;
#ifdef GTE_LOG
    GTE_LOG("GTE CC\n");
#endif
    gteFLAG = 0;

    //tmpMAC1 = A1(((s64)gteRBK) + (((s32)gteLR1 * (s32)gteIR1 + (s32)gteLR2 * (s32)gteIR2 + (s32)gteLR3 * (s32)gteIR3) >> 12));
    //tmpMAC2 = A2(((s64)gteGBK) + (((s32)gteLG1 * (s32)gteIR1 + (s32)gteLG2 * (s32)gteIR2 + (s32)gteLG3 * (s32)gteIR3) >> 12));
    //tmpMAC3 = A3(((s64)gteBBK) + (((s32)gteLB1 * (s32)gteIR1 + (s32)gteLB2 * (s32)gteIR2 + (s32)gteLB3 * (s32)gteIR3) >> 12));
    guVecMultiplySR(&gteLR1, &gteIR1, tmpMAC);
    tmpMAC1 = A1(((s64)gteRBK) + (tmpMAC[0] >> 12));
    tmpMAC2 = A2(((s64)gteGBK) + (tmpMAC[1] >> 12));
    tmpMAC3 = A3(((s64)gteBBK) + (tmpMAC[2] >> 12));

    gteIR1 = limB1(tmpMAC1, 1);
    gteIR2 = limB2(tmpMAC2, 1);
    gteIR3 = limB3(tmpMAC3, 1);
    tmpMAC1 = ((s32)gteR * gteIR1) >> 8;
    tmpMAC2 = ((s32)gteG * gteIR2) >> 8;
    tmpMAC3 = ((s32)gteB * gteIR3) >> 8;
    gteIR1 = limB1(tmpMAC1, 1);
    gteIR2 = limB2(tmpMAC2, 1);
    gteIR3 = limB3(tmpMAC3, 1);

    gteRGB0 = gteRGB1;
    gteRGB1 = gteRGB2;
    gteCODE2 = gteCODE;
    gteR2 = limC1(tmpMAC1 >> 4);
    gteG2 = limC2(tmpMAC2 >> 4);
    gteB2 = limC3(tmpMAC3 >> 4);

    RESET_MAC();
}

#define INTPL_FOOTER(lm) \
    gteIR1 = limB1(tmpMAC1, lm); \
    gteIR2 = limB2(tmpMAC2, lm); \
    gteIR3 = limB3(tmpMAC3, lm); \
    gteRGB0 = gteRGB1; \
    gteRGB1 = gteRGB2; \
    gteCODE2 = gteCODE; \
    gteR2 = limC1(tmpMAC1 >> 4); \
    gteG2 = limC2(tmpMAC2 >> 4); \
    gteB2 = limC3(tmpMAC3 >> 4); \
     \
    RESET_MAC();

void gteINTPL(psxCP2Regs *regs) {
    s32 tmpMAC1, tmpMAC2, tmpMAC3;
    gteFLAG = 0;

    if (psxRegs.code & 0x80000) {
        tmpMAC1 = ((gteIR1 << 12) + (gteIR0 * limB1(A1U((s64)gteRFC - gteIR1), 0))) >> 12;
        tmpMAC2 = ((gteIR2 << 12) + (gteIR0 * limB2(A2U((s64)gteGFC - gteIR2), 0))) >> 12;
        tmpMAC3 = ((gteIR3 << 12) + (gteIR0 * limB3(A3U((s64)gteBFC - gteIR3), 0))) >> 12;
    } else {
        tmpMAC1 = ((gteIR1 << 12) + (gteIR0 * limB1(A1U((s64)gteRFC - gteIR1), 0)));
        tmpMAC2 = ((gteIR2 << 12) + (gteIR0 * limB2(A2U((s64)gteGFC - gteIR2), 0)));
        tmpMAC3 = ((gteIR3 << 12) + (gteIR0 * limB3(A3U((s64)gteBFC - gteIR3), 0)));
    }

    if (psxRegs.code & 0x400)
    {
        INTPL_FOOTER(1);
    }
    else
    {
        INTPL_FOOTER(0);
    }
}

void gteCDP(psxCP2Regs *regs) {
    s32 tmpMAC[3];
    s32 tmpMAC1, tmpMAC2, tmpMAC3;
#ifdef GTE_LOG
    GTE_LOG("GTE CDP\n");
#endif
    gteFLAG = 0;

    //tmpMAC1 = A1(((s64)gteRBK) + (((s32)gteLR1 * (s32)gteIR1 + (s32)gteLR2 * (s32)gteIR2 + (s32)gteLR3 * (s32)gteIR3) >> 12));
    //tmpMAC2 = A2(((s64)gteGBK) + (((s32)gteLG1 * (s32)gteIR1 + (s32)gteLG2 * (s32)gteIR2 + (s32)gteLG3 * (s32)gteIR3) >> 12));
    //tmpMAC3 = A3(((s64)gteBBK) + (((s32)gteLB1 * (s32)gteIR1 + (s32)gteLB2 * (s32)gteIR2 + (s32)gteLB3 * (s32)gteIR3) >> 12));
    guVecMultiplySR(&gteLR1, &gteIR1, tmpMAC);
    tmpMAC1 = A1(((s64)gteRBK) + (tmpMAC[0] >> 12));
    tmpMAC2 = A2(((s64)gteGBK) + (tmpMAC[1] >> 12));
    tmpMAC3 = A3(((s64)gteBBK) + (tmpMAC[2] >> 12));

    gteIR1 = limB1(tmpMAC1, 1);
    gteIR2 = limB2(tmpMAC2, 1);
    gteIR3 = limB3(tmpMAC3, 1);
    tmpMAC1 = (((gteR << 4) * gteIR1) + (gteIR0 * limB1(A1U((s64)gteRFC - ((gteR * gteIR1) >> 8)), 0))) >> 12;
    tmpMAC2 = (((gteG << 4) * gteIR2) + (gteIR0 * limB2(A2U((s64)gteGFC - ((gteG * gteIR2) >> 8)), 0))) >> 12;
    tmpMAC3 = (((gteB << 4) * gteIR3) + (gteIR0 * limB3(A3U((s64)gteBFC - ((gteB * gteIR3) >> 8)), 0))) >> 12;
    gteIR1 = limB1(tmpMAC1, 1);
    gteIR2 = limB2(tmpMAC2, 1);
    gteIR3 = limB3(tmpMAC3, 1);

    gteRGB0 = gteRGB1;
    gteRGB1 = gteRGB2;
    gteCODE2 = gteCODE;
    gteR2 = limC1(tmpMAC1 >> 4);
    gteG2 = limC2(tmpMAC2 >> 4);
    gteB2 = limC3(tmpMAC3 >> 4);

    RESET_MAC();
}
