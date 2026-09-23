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

#define VX(n) (n < 3 ? regs->CP2D.p[n << 1].sw.l : regs->CP2D.p[9].sw.l)
#define VY(n) (n < 3 ? regs->CP2D.p[n << 1].sw.h : regs->CP2D.p[10].sw.l)
#define VZ(n) (n < 3 ? regs->CP2D.p[(n << 1) + 1].sw.l : regs->CP2D.p[11].sw.l)
#define MX11(n) (n < 3 ? regs->CP2C.p[(n << 3)].sw.l : 0)
#define MX12(n) (n < 3 ? regs->CP2C.p[(n << 3)].sw.h : 0)
#define MX13(n) (n < 3 ? regs->CP2C.p[(n << 3) + 1].sw.l : 0)
#define MX21(n) (n < 3 ? regs->CP2C.p[(n << 3) + 1].sw.h : 0)
#define MX22(n) (n < 3 ? regs->CP2C.p[(n << 3) + 2].sw.l : 0)
#define MX23(n) (n < 3 ? regs->CP2C.p[(n << 3) + 2].sw.h : 0)
#define MX31(n) (n < 3 ? regs->CP2C.p[(n << 3) + 3].sw.l : 0)
#define MX32(n) (n < 3 ? regs->CP2C.p[(n << 3) + 3].sw.h : 0)
#define MX33(n) (n < 3 ? regs->CP2C.p[(n << 3) + 4].sw.l : 0)
#define CV1(n) (n < 3 ? (s32)regs->CP2C.r[(n << 3) + 5] : 0)
#define CV2(n) (n < 3 ? (s32)regs->CP2C.r[(n << 3) + 6] : 0)
#define CV3(n) (n < 3 ? (s32)regs->CP2C.r[(n << 3) + 7] : 0)

#define fSX(n) ((regs->CP2D.p)[((n) + 12)].sw.l)
#define fSY(n) ((regs->CP2D.p)[((n) + 12)].sw.h)
#define fSZ(n) ((regs->CP2D.p)[((n) + 17)].w.l) /* (n == 0) => SZ1; */

#define gteVXY0 (regs->CP2D.r[0])
#define gteVX0  (regs->CP2D.p[0].sw.l)
#define gteVY0  (regs->CP2D.p[0].sw.h)
#define gteVZ0  (regs->CP2D.p[1].sw.l)
#define gteVXY1 (regs->CP2D.r[2])
#define gteVX1  (regs->CP2D.p[2].sw.l)
#define gteVY1  (regs->CP2D.p[2].sw.h)
#define gteVZ1  (regs->CP2D.p[3].sw.l)
#define gteVXY2 (regs->CP2D.r[4])
#define gteVX2  (regs->CP2D.p[4].sw.l)
#define gteVY2  (regs->CP2D.p[4].sw.h)
#define gteVZ2  (regs->CP2D.p[5].sw.l)
#define gteRGB  (regs->CP2D.r[6])
#define gteR    (regs->CP2D.p[6].b.l)
#define gteG    (regs->CP2D.p[6].b.h)
#define gteB    (regs->CP2D.p[6].b.h2)
#define gteCODE (regs->CP2D.p[6].b.h3)
#define gteOTZ  (regs->CP2D.p[7].w.l)
#define gteIR0  (regs->CP2D.p[8].sw.l)
#define gteIR1  (regs->CP2D.p[9].sw.l)
#define gteIR2  (regs->CP2D.p[10].sw.l)
#define gteIR3  (regs->CP2D.p[11].sw.l)
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

#define gteR11R12 (((s32 *)regs->CP2C.r)[0])
#define gteR22R23 (((s32 *)regs->CP2C.r)[2])
#define gteR11 (regs->CP2C.p[0].sw.l)
#define gteR12 (regs->CP2C.p[0].sw.h)
#define gteR13 (regs->CP2C.p[1].sw.l)
#define gteR21 (regs->CP2C.p[1].sw.h)
#define gteR22 (regs->CP2C.p[2].sw.l)
#define gteR23 (regs->CP2C.p[2].sw.h)
#define gteR31 (regs->CP2C.p[3].sw.l)
#define gteR32 (regs->CP2C.p[3].sw.h)
#define gteR33 (regs->CP2C.p[4].sw.l)
#define gteTRX (((s32 *)regs->CP2C.r)[5])
#define gteTRY (((s32 *)regs->CP2C.r)[6])
#define gteTRZ (((s32 *)regs->CP2C.r)[7])
#define gteL11 (regs->CP2C.p[8].sw.l)
#define gteL12 (regs->CP2C.p[8].sw.h)
#define gteL13 (regs->CP2C.p[9].sw.l)
#define gteL21 (regs->CP2C.p[9].sw.h)
#define gteL22 (regs->CP2C.p[10].sw.l)
#define gteL23 (regs->CP2C.p[10].sw.h)
#define gteL31 (regs->CP2C.p[11].sw.l)
#define gteL32 (regs->CP2C.p[11].sw.h)
#define gteL33 (regs->CP2C.p[12].sw.l)
#define gteRBK (((s32 *)regs->CP2C.r)[13])
#define gteGBK (((s32 *)regs->CP2C.r)[14])
#define gteBBK (((s32 *)regs->CP2C.r)[15])
#define gteLR1 (regs->CP2C.p[16].sw.l)
#define gteLR2 (regs->CP2C.p[16].sw.h)
#define gteLR3 (regs->CP2C.p[17].sw.l)
#define gteLG1 (regs->CP2C.p[17].sw.h)
#define gteLG2 (regs->CP2C.p[18].sw.l)
#define gteLG3 (regs->CP2C.p[18].sw.h)
#define gteLB1 (regs->CP2C.p[19].sw.l)
#define gteLB2 (regs->CP2C.p[19].sw.h)
#define gteLB3 (regs->CP2C.p[20].sw.l)
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
        gteFLAG |= (1U << 31) | (1U << 17);
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

#define A1(a) BOUNDS((a), 0x7fffffff, (1U << 30), -(s64)0x80000000, (1U << 31) | (1U << 27))
#define A2(a) BOUNDS((a), 0x7fffffff, (1U << 29), -(s64)0x80000000, (1U << 31) | (1U << 26))
#define A3(a) BOUNDS((a), 0x7fffffff, (1U << 28), -(s64)0x80000000, (1U << 31) | (1U << 25))
#define limB1(a, l) LIM((a), 0x7fff, -0x8000 * !l, (1U << 31) | (1U << 24))
#define limB2(a, l) LIM((a), 0x7fff, -0x8000 * !l, (1U << 31) | (1U << 23))
#define limB3(a, l) LIM((a), 0x7fff, -0x8000 * !l, (1U << 22))
#define limC1(a) LIM((a), 0x00ff, 0x0000, (1U << 21))
#define limC2(a) LIM((a), 0x00ff, 0x0000, (1U << 20))
#define limC3(a) LIM((a), 0x00ff, 0x0000, (1U << 19))
#define limD(a) LIM((a), 0xffff, 0x0000, (1U << 31) | (1U << 18))

#define F(a) BOUNDS((a), 0x7fffffff, (1U << 31) | (1U << 16), -(s64)0x80000000, (1U << 31) | (1U << 15))
#define limG1(a) LIM((a), 0x3ff, -0x400, (1U << 31) | (1U << 14))
#define limG2(a) LIM((a), 0x3ff, -0x400, (1U << 31) | (1U << 13))
//Fix for Valkyrie Profile crash loading world map
// (PCSX Rearmed commit 7384197d8a5fd20a4d94f3517a6462f7fe86dd4c
//  'seems to work, unverified value')
//#define limH(a) LIM((a), 0xfff, 0x000, (1 << 12))
#define limH(a) LIM((a), 0x1000, 0x0000, (1U << 12))

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
    switch (reg) {
        case 1:
        case 3:
        case 5:
        case 8:
        case 9:
        case 10:
        case 11:
            psxRegs.CP2D.r[reg] = (s32)psxRegs.CP2D.p[reg].sw.l;
            break;

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
        case 4:
        case 12:
        case 20:
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
    psxRegs.GPR.r[_Rt_] = psxRegs.CP2C.r[_Rd_];
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

/*
 * Accurate common helpers for GTE commands which use the 44-bit MAC1-MAC3
 * accumulators.  Overflow is checked at the hardware-defined stages before
 * values are truncated to their 32-bit register representation.
 */
#define GTE_MAC123_MIN (-(s64)(1ULL << 43))
#define GTE_MAC123_MAX ((s64)((1ULL << 43) - 1))
#define GTE_FLAG_ERROR_MASK 0x7f87e000U

#if defined(__GNUC__)
#define GTE_NOINLINE __attribute__((noinline, noclone))
#else
#define GTE_NOINLINE
#endif

static inline void gteCheckMAC123Overflow(psxCP2Regs *regs, int index, s64 value) {
    static const u32 overflow_flags[3] = { 1U << 30, 1U << 29, 1U << 28 };
    static const u32 underflow_flags[3] = { 1U << 27, 1U << 26, 1U << 25 };

    if (value > GTE_MAC123_MAX)
        gteFLAG |= overflow_flags[index];
    else if (value < GTE_MAC123_MIN)
        gteFLAG |= underflow_flags[index];
}

static inline s64 gteSignExtendMAC123(psxCP2Regs *regs, int index, s64 value) {
    const u64 mask = (1ULL << 44) - 1;
    const u64 sign = 1ULL << 43;
    const u64 wrapped = (u64)value & mask;

    gteCheckMAC123Overflow(regs, index, value);
    return (s64)(wrapped ^ sign) - (s64)sign;
}

static inline s32 gteLimitIR123LM0(psxCP2Regs *regs, s32 value, u32 flag) {
    if (value > 0x7fff) {
        gteFLAG |= flag;
        return 0x7fff;
    }
    if (value < -0x8000) {
        gteFLAG |= flag;
        return -0x8000;
    }
    return value;
}

static inline s32 gteLimitIR123LM1(psxCP2Regs *regs, s32 value, u32 flag) {
    if (value > 0x7fff) {
        gteFLAG |= flag;
        return 0x7fff;
    }
    if (value < 0) {
        gteFLAG |= flag;
        return 0;
    }
    return value;
}

static inline s32 gteLimitIR123(psxCP2Regs *regs, s32 value, int lm, u32 flag) {
    if (lm)
        return gteLimitIR123LM1(regs, value, flag);
    return gteLimitIR123LM0(regs, value, flag);
}

static inline s32 gteShiftMAC123(s64 value, int shift) {
    if (shift == 12)
        return (s32)(value >> 12);
    return (s32)value;
}

static inline s32 gteSetMAC123(psxCP2Regs *regs, int index, s64 value, int shift) {
    s32 result;

    gteCheckMAC123Overflow(regs, index, value);
    result = gteShiftMAC123(value, shift);
    ((s32 *)regs->CP2D.r)[25 + index] = result;
    return result;
}

static inline s32 gteSetIR123(psxCP2Regs *regs, int index, s32 value, int lm) {
    static const u32 saturation_flags[3] = { 1U << 24, 1U << 23, 1U << 22 };
    const s32 result = gteLimitIR123(regs, value, lm, saturation_flags[index]);

    regs->CP2D.p[9 + index].sw.l = (s16)result;
    return result;
}

static inline void gteSetMACAndIR123(psxCP2Regs *regs, int index, s64 value,
                                     int shift, int lm) {
    gteSetIR123(regs, index, gteSetMAC123(regs, index, value, shift), lm);
}

static inline void gteCheckMAC0Overflow(psxCP2Regs *regs, s64 value) {
    if (value > 0x7fffffffLL)
        gteFLAG |= 1U << 16;
    else if (value < -0x80000000LL)
        gteFLAG |= 1U << 15;
}

static inline s32 gteSetMAC0(psxCP2Regs *regs, s64 value) {
    gteCheckMAC0Overflow(regs, value);
    gteMAC0 = (s32)value;
    return gteMAC0;
}

static inline void gteUpdateErrorFlag(psxCP2Regs *regs) {
    if (gteFLAG & GTE_FLAG_ERROR_MASK)
        gteFLAG |= 1U << 31;
}

static inline void gtePushRGBFromMAC(psxCP2Regs *regs) {
    gteRGB0 = gteRGB1;
    gteRGB1 = gteRGB2;
    gteCODE2 = gteCODE;
    gteR2 = (u8)limC1(gteMAC1 >> 4);
    gteG2 = (u8)limC2(gteMAC2 >> 4);
    gteB2 = (u8)limC3(gteMAC3 >> 4);
}

/* Execute the common transform/projection part for one RTPS/RTPT vertex. */
static inline u32 gteRTPSVertex(psxCP2Regs *regs, s32 vx, s32 vy, s32 vz,
                               int shift, int lm, u16 *sz, s16 *sx, s16 *sy) {
    s64 x, y, z;
    s64 screen_x, screen_y;
    u32 quotient;

    x = gteSignExtendMAC123(regs, 0,
            (s64)gteTRX * 4096 + (s64)gteR11 * vx);
    x = gteSignExtendMAC123(regs, 0, x + (s64)gteR12 * vy);
    x += (s64)gteR13 * vz;

    y = gteSignExtendMAC123(regs, 1,
            (s64)gteTRY * 4096 + (s64)gteR21 * vx);
    y = gteSignExtendMAC123(regs, 1, y + (s64)gteR22 * vy);
    y += (s64)gteR23 * vz;

    z = gteSignExtendMAC123(regs, 2,
            (s64)gteTRZ * 4096 + (s64)gteR31 * vx);
    z = gteSignExtendMAC123(regs, 2, z + (s64)gteR32 * vy);
    z += (s64)gteR33 * vz;

    /* The final add is checked when MAC is stored, without an extra 44-bit wrap. */
    gteSetMAC123(regs, 0, x, shift);
    gteSetMAC123(regs, 1, y, shift);
    gteSetMAC123(regs, 2, z, shift);

    gteSetIR123(regs, 0, gteMAC1, lm);
    gteSetIR123(regs, 1, gteMAC2, lm);

    /*
     * RTP has unusual IR3 flag behaviour: saturation is tested against
     * Z>>12 regardless of SF, while the stored IR3 is clamped from MAC3.
     */
    (void)gteLimitIR123(regs, (s32)(z >> 12), 0, 1U << 22);
    gteIR3 = (s16)gteLimitIR123(regs, gteMAC3, lm, 0);

    *sz = (u16)limD((s32)(z >> 12));
    quotient = limE(DIVIDE_INT(gteH, *sz));

    screen_x = (s64)gteOFX + (s64)gteIR1 * quotient;
    screen_y = (s64)gteOFY + (s64)gteIR2 * quotient;
    gteCheckMAC0Overflow(regs, screen_x);
    gteCheckMAC0Overflow(regs, screen_y);
    *sx = (s16)limG1((s32)(screen_x >> 16));
    *sy = (s16)limG2((s32)(screen_y >> 16));

    return quotient;
}

static inline void gteRTPSDepthCue(psxCP2Regs *regs, u32 quotient) {
    const s64 value = (s64)gteDQB + (s64)gteDQA * quotient;

    gteSetMAC0(regs, value);
    gteIR0 = (s16)limH((s32)(value >> 12));
}

/*
 * Keep the three-vertex RTPT body shared.  noinline/noclone is intentional:
 * otherwise PPC LTO expands the complete transform three times in gteRTPT.
 * The one-vertex RTPS command still gets the original inline hot path.
 */
static GTE_NOINLINE u32 gteRTPTVertex(psxCP2Regs *regs, s32 vx, s32 vy,
                                     s32 vz, int shift, int lm, u16 *sz,
                                     s16 *sx, s16 *sy) {
    return gteRTPSVertex(regs, vx, vy, vz, shift, lm, sz, sx, sy);
}



void gteRTPS(psxCP2Regs *regs) {
    const int shift = GTE_SF(gteop) ? 12 : 0;
    const int lm = GTE_LM(gteop);
    u32 quotient;

#ifdef GTE_LOG
    GTE_LOG("GTE RTPS\n");
#endif
    gteFLAG = 0;

    gteSZ0 = gteSZ1;
    gteSZ1 = gteSZ2;
    gteSZ2 = gteSZ3;
    gteSXY0 = gteSXY1;
    gteSXY1 = gteSXY2;
    quotient = gteRTPSVertex(regs, gteVX0, gteVY0, gteVZ0, shift, lm,
                             &gteSZ3, &gteSX2, &gteSY2);
    gteRTPSDepthCue(regs, quotient);
    gteUpdateErrorFlag(regs);
}

void gteRTPT(psxCP2Regs *regs) {
    const int shift = GTE_SF(gteop) ? 12 : 0;
    const int lm = GTE_LM(gteop);
    u32 quotient = 0;
    int v;

#ifdef GTE_LOG
    GTE_LOG("GTE RTPT\n");
#endif
    gteFLAG = 0;

    gteSZ0 = gteSZ3;
    for (v = 0; v < 3; v++) {
        quotient = gteRTPTVertex(regs, VX(v), VY(v), VZ(v), shift, lm,
                                 &fSZ(v), &fSX(v), &fSY(v));
    }

    gteRTPSDepthCue(regs, quotient);
    gteUpdateErrorFlag(regs);
}

static inline void gteMVMVANormal(psxCP2Regs *regs, const s16 matrix[9],
                                  const s32 translation[3], s32 vx, s32 vy,
                                  s32 vz, int shift, int lm) {
    int i;

    for (i = 0; i < 3; i++) {
        s64 value = gteSignExtendMAC123(regs, i,
                (s64)translation[i] * 4096 + (s64)matrix[i * 3] * vx);
        value = gteSignExtendMAC123(regs, i,
                value + (s64)matrix[i * 3 + 1] * vy);
        value += (s64)matrix[i * 3 + 2] * vz;
        gteSetMACAndIR123(regs, i, value, shift, lm);
    }
}

/*
 * MVMVA with the far-color translation vector follows a hardware-bug path:
 * T + Mx*Vx only affects intermediate IR saturation flags, while the final
 * MAC/IR result contains My*Vy + Mz*Vz.
 */
static inline void gteMVMVAFarColorBug(psxCP2Regs *regs, const s16 matrix[9],
                                      const s32 translation[3], s32 vx, s32 vy,
                                      s32 vz, int shift, int lm) {
    static const u32 saturation_flags[3] = { 1U << 24, 1U << 23, 1U << 22 };
    int i;

    for (i = 0; i < 3; i++) {
        s64 intermediate = gteSignExtendMAC123(regs, i,
                (s64)translation[i] * 4096 + (s64)matrix[i * 3] * vx);
        s64 value;

        (void)gteLimitIR123(regs, gteShiftMAC123(intermediate, shift), 0,
                            saturation_flags[i]);
        value = gteSignExtendMAC123(regs, i,
                (s64)matrix[i * 3 + 1] * vy);
        value += (s64)matrix[i * 3 + 2] * vz;
        gteSetMACAndIR123(regs, i, value, shift, lm);
    }
}

void gteMVMVA(psxCP2Regs *regs) {
    const int shift = GTE_SF(gteop) ? 12 : 0;
    const int mx = GTE_MX(gteop);
    const int v = GTE_V(gteop);
    const int cv = GTE_CV(gteop);
    const int lm = GTE_LM(gteop);
    const s32 vx = VX(v);
    const s32 vy = VY(v);
    const s32 vz = VZ(v);
    s16 matrix[9];
    s32 translation[3];

#ifdef GTE_LOG
    GTE_LOG("GTE MVMVA\n");
#endif
    gteFLAG = 0;

    if (mx < 3) {
        matrix[0] = MX11(mx);
        matrix[1] = MX12(mx);
        matrix[2] = MX13(mx);
        matrix[3] = MX21(mx);
        matrix[4] = MX22(mx);
        matrix[5] = MX23(mx);
        matrix[6] = MX31(mx);
        matrix[7] = MX32(mx);
        matrix[8] = MX33(mx);
    } else {
        /* Undocumented MX=3 matrix generated internally by the GTE. */
        matrix[0] = -(s16)((u16)gteR << 4);
        matrix[1] = (s16)((u16)gteR << 4);
        matrix[2] = gteIR0;
        matrix[3] = matrix[4] = matrix[5] = gteR13;
        matrix[6] = matrix[7] = matrix[8] = gteR22;
    }

    translation[0] = CV1(cv);
    translation[1] = CV2(cv);
    translation[2] = CV3(cv);

    if (cv == 2)
        gteMVMVAFarColorBug(regs, matrix, translation, vx, vy, vz, shift, lm);
    else
        gteMVMVANormal(regs, matrix, translation, vx, vy, vz, shift, lm);

    gteUpdateErrorFlag(regs);
}

void gteNCLIP(psxCP2Regs *regs) {
    s64 value;

#ifdef GTE_LOG
    GTE_LOG("GTE NCLIP\n");
#endif
    gteFLAG = 0;

    value = (s64)gteSX0 * gteSY1 + (s64)gteSX1 * gteSY2 +
            (s64)gteSX2 * gteSY0 - (s64)gteSX0 * gteSY2 -
            (s64)gteSX1 * gteSY0 - (s64)gteSX2 * gteSY1;
    gteSetMAC0(regs, value);
    gteUpdateErrorFlag(regs);
}

void gteAVSZ3(psxCP2Regs *regs) {
    s64 value;

#ifdef GTE_LOG
    GTE_LOG("GTE AVSZ3\n");
#endif
    gteFLAG = 0;

    value = (s64)gteZSF3 * ((u32)gteSZ1 + (u32)gteSZ2 + (u32)gteSZ3);
    gteSetMAC0(regs, value);
    gteOTZ = (u16)limD((s32)(value >> 12));
    gteUpdateErrorFlag(regs);
}

void gteAVSZ4(psxCP2Regs *regs) {
    s64 value;

#ifdef GTE_LOG
    GTE_LOG("GTE AVSZ4\n");
#endif
    gteFLAG = 0;

    value = (s64)gteZSF4 * ((u32)gteSZ0 + (u32)gteSZ1 +
                            (u32)gteSZ2 + (u32)gteSZ3);
    gteSetMAC0(regs, value);
    gteOTZ = (u16)limD((s32)(value >> 12));
    gteUpdateErrorFlag(regs);
}

void gteSQR(psxCP2Regs *regs) {
    const int shift = GTE_SF(gteop) ? 12 : 0;
    const int lm = GTE_LM(gteop);
    const s32 ir1 = gteIR1;
    const s32 ir2 = gteIR2;
    const s32 ir3 = gteIR3;

#ifdef GTE_LOG
    GTE_LOG("GTE SQR\n");
#endif
    gteFLAG = 0;

    gteSetMACAndIR123(regs, 0, (s64)ir1 * ir1, shift, lm);
    gteSetMACAndIR123(regs, 1, (s64)ir2 * ir2, shift, lm);
    gteSetMACAndIR123(regs, 2, (s64)ir3 * ir3, shift, lm);
    gteUpdateErrorFlag(regs);
}

static inline void gteApplyLightMatrix(psxCP2Regs *regs, s32 vx, s32 vy,
                                       s32 vz, int shift, int lm) {
    s64 value;

    value = gteSignExtendMAC123(regs, 0,
            (s64)gteL11 * vx + (s64)gteL12 * vy);
    gteSetMACAndIR123(regs, 0, value + (s64)gteL13 * vz, shift, lm);
    value = gteSignExtendMAC123(regs, 1,
            (s64)gteL21 * vx + (s64)gteL22 * vy);
    gteSetMACAndIR123(regs, 1, value + (s64)gteL23 * vz, shift, lm);
    value = gteSignExtendMAC123(regs, 2,
            (s64)gteL31 * vx + (s64)gteL32 * vy);
    gteSetMACAndIR123(regs, 2, value + (s64)gteL33 * vz, shift, lm);
}

static inline void gteApplyColorMatrix(psxCP2Regs *regs, int shift, int lm) {
    const s32 ir1 = gteIR1;
    const s32 ir2 = gteIR2;
    const s32 ir3 = gteIR3;
    s64 value;

    value = gteSignExtendMAC123(regs, 0,
            (s64)gteRBK * 4096 + (s64)gteLR1 * ir1);
    value = gteSignExtendMAC123(regs, 0, value + (s64)gteLR2 * ir2);
    gteSetMACAndIR123(regs, 0, value + (s64)gteLR3 * ir3, shift, lm);
    value = gteSignExtendMAC123(regs, 1,
            (s64)gteGBK * 4096 + (s64)gteLG1 * ir1);
    value = gteSignExtendMAC123(regs, 1, value + (s64)gteLG2 * ir2);
    gteSetMACAndIR123(regs, 1, value + (s64)gteLG3 * ir3, shift, lm);
    value = gteSignExtendMAC123(regs, 2,
            (s64)gteBBK * 4096 + (s64)gteLB1 * ir1);
    value = gteSignExtendMAC123(regs, 2, value + (s64)gteLB2 * ir2);
    gteSetMACAndIR123(regs, 2, value + (s64)gteLB3 * ir3, shift, lm);
}

static inline void gteGetColorProducts(psxCP2Regs *regs, s64 products[3]) {
    products[0] = (s64)gteR * gteIR1 * 16;
    products[1] = (s64)gteG * gteIR2 * 16;
    products[2] = (s64)gteB * gteIR3 * 16;
}

static inline void gteMultiplyColor(psxCP2Regs *regs, int shift, int lm) {
    s64 products[3];

    gteGetColorProducts(regs, products);
    gteSetMACAndIR123(regs, 0, products[0], shift, lm);
    gteSetMACAndIR123(regs, 1, products[1], shift, lm);
    gteSetMACAndIR123(regs, 2, products[2], shift, lm);
}

static inline void gteInterpolateColor(psxCP2Regs *regs, s64 in1, s64 in2,
                                       s64 in3, int shift, int lm) {
    const s32 ir0 = gteIR0;
    s32 ir1, ir2, ir3;

    gteSetMACAndIR123(regs, 0, (s64)gteRFC * 4096 - in1, shift, 0);
    gteSetMACAndIR123(regs, 1, (s64)gteGFC * 4096 - in2, shift, 0);
    gteSetMACAndIR123(regs, 2, (s64)gteBFC * 4096 - in3, shift, 0);
    ir1 = gteIR1;
    ir2 = gteIR2;
    ir3 = gteIR3;
    gteSetMACAndIR123(regs, 0, (s64)ir1 * ir0 + in1, shift, lm);
    gteSetMACAndIR123(regs, 1, (s64)ir2 * ir0 + in2, shift, lm);
    gteSetMACAndIR123(regs, 2, (s64)ir3 * ir0 + in3, shift, lm);
}

static inline void gteNCSVertex(psxCP2Regs *regs, s32 vx, s32 vy, s32 vz,
                                int shift, int lm) {
    gteApplyLightMatrix(regs, vx, vy, vz, shift, lm);
    gteApplyColorMatrix(regs, shift, lm);
    gtePushRGBFromMAC(regs);
}

static inline void gteNCCSVertex(psxCP2Regs *regs, s32 vx, s32 vy, s32 vz,
                                 int shift, int lm) {
    gteApplyLightMatrix(regs, vx, vy, vz, shift, lm);
    gteApplyColorMatrix(regs, shift, lm);
    gteMultiplyColor(regs, shift, lm);
    gtePushRGBFromMAC(regs);
}

static inline void gteNCDSVertex(psxCP2Regs *regs, s32 vx, s32 vy, s32 vz,
                                 int shift, int lm) {
    s64 products[3];

    gteApplyLightMatrix(regs, vx, vy, vz, shift, lm);
    gteApplyColorMatrix(regs, shift, lm);
    gteGetColorProducts(regs, products);
    gteInterpolateColor(regs, products[0], products[1], products[2], shift, lm);
    gtePushRGBFromMAC(regs);
}

/* Share the complete per-vertex color pipelines in the triple commands. */
static GTE_NOINLINE void gteNCTVertex(psxCP2Regs *regs, s32 vx, s32 vy,
                                     s32 vz, int shift, int lm) {
    gteNCSVertex(regs, vx, vy, vz, shift, lm);
}

static GTE_NOINLINE void gteNCCTVertex(psxCP2Regs *regs, s32 vx, s32 vy,
                                      s32 vz, int shift, int lm) {
    gteNCCSVertex(regs, vx, vy, vz, shift, lm);
}

static GTE_NOINLINE void gteNCDTVertex(psxCP2Regs *regs, s32 vx, s32 vy,
                                      s32 vz, int shift, int lm) {
    gteNCDSVertex(regs, vx, vy, vz, shift, lm);
}

void gteNCCS(psxCP2Regs *regs) {
    const int shift = GTE_SF(gteop) ? 12 : 0;
    const int lm = GTE_LM(gteop);

#ifdef GTE_LOG
    GTE_LOG("GTE NCCS\n");
#endif
    gteFLAG = 0;
    gteNCCSVertex(regs, gteVX0, gteVY0, gteVZ0, shift, lm);
    gteUpdateErrorFlag(regs);
}

void gteNCCT(psxCP2Regs *regs) {
    const int shift = GTE_SF(gteop) ? 12 : 0;
    const int lm = GTE_LM(gteop);
    int v;

#ifdef GTE_LOG
    GTE_LOG("GTE NCCT\n");
#endif
    gteFLAG = 0;
    for (v = 0; v < 3; v++)
        gteNCCTVertex(regs, VX(v), VY(v), VZ(v), shift, lm);
    gteUpdateErrorFlag(regs);
}

void gteNCDS(psxCP2Regs *regs) {
    const int shift = GTE_SF(gteop) ? 12 : 0;
    const int lm = GTE_LM(gteop);

#ifdef GTE_LOG
    GTE_LOG("GTE NCDS\n");
#endif
    gteFLAG = 0;
    gteNCDSVertex(regs, gteVX0, gteVY0, gteVZ0, shift, lm);
    gteUpdateErrorFlag(regs);
}

void gteNCDT(psxCP2Regs *regs) {
    const int shift = GTE_SF(gteop) ? 12 : 0;
    const int lm = GTE_LM(gteop);
    int v;

#ifdef GTE_LOG
    GTE_LOG("GTE NCDT\n");
#endif
    gteFLAG = 0;
    for (v = 0; v < 3; v++)
        gteNCDTVertex(regs, VX(v), VY(v), VZ(v), shift, lm);
    gteUpdateErrorFlag(regs);
}

void gteOP(psxCP2Regs *regs) {
    const int shift = GTE_SF(gteop) ? 12 : 0;
    const int lm = GTE_LM(gteop);
    const s32 d1 = gteR11;
    const s32 d2 = gteR22;
    const s32 d3 = gteR33;
    const s32 ir1 = gteIR1;
    const s32 ir2 = gteIR2;
    const s32 ir3 = gteIR3;

#ifdef GTE_LOG
    GTE_LOG("GTE OP\n");
#endif
    gteFLAG = 0;

    gteSetMACAndIR123(regs, 0, (s64)ir3 * d2 - (s64)ir2 * d3,
                      shift, lm);
    gteSetMACAndIR123(regs, 1, (s64)ir1 * d3 - (s64)ir3 * d1,
                      shift, lm);
    gteSetMACAndIR123(regs, 2, (s64)ir2 * d1 - (s64)ir1 * d2,
                      shift, lm);
    gteUpdateErrorFlag(regs);
}

void gteDCPL(psxCP2Regs *regs) {
    const int shift = GTE_SF(gteop) ? 12 : 0;
    const int lm = GTE_LM(gteop);
    s64 products[3];

#ifdef GTE_LOG
    GTE_LOG("GTE DCPL\n");
#endif
    gteFLAG = 0;
    gteGetColorProducts(regs, products);
    gteInterpolateColor(regs, products[0], products[1], products[2], shift, lm);
    gtePushRGBFromMAC(regs);
    gteUpdateErrorFlag(regs);
}

void gteGPF(psxCP2Regs *regs) {
    const int shift = GTE_SF(gteop) ? 12 : 0;
    const int lm = GTE_LM(gteop);
    const s32 ir0 = gteIR0;
    const s32 ir1 = gteIR1;
    const s32 ir2 = gteIR2;
    const s32 ir3 = gteIR3;

#ifdef GTE_LOG
    GTE_LOG("GTE GPF\n");
#endif
    gteFLAG = 0;

    gteSetMACAndIR123(regs, 0, (s64)ir0 * ir1, shift, lm);
    gteSetMACAndIR123(regs, 1, (s64)ir0 * ir2, shift, lm);
    gteSetMACAndIR123(regs, 2, (s64)ir0 * ir3, shift, lm);
    gtePushRGBFromMAC(regs);
    gteUpdateErrorFlag(regs);
}

void gteGPL(psxCP2Regs *regs) {
    const int shift = GTE_SF(gteop) ? 12 : 0;
    const int lm = GTE_LM(gteop);
    const s32 ir0 = gteIR0;
    const s32 ir1 = gteIR1;
    const s32 ir2 = gteIR2;
    const s32 ir3 = gteIR3;
    const s32 mac1 = gteMAC1;
    const s32 mac2 = gteMAC2;
    const s32 mac3 = gteMAC3;
    const s64 scale = (shift == 12) ? 4096 : 1;

#ifdef GTE_LOG
    GTE_LOG("GTE GPL\n");
#endif
    gteFLAG = 0;

    gteSetMACAndIR123(regs, 0, (s64)mac1 * scale + (s64)ir0 * ir1,
                      shift, lm);
    gteSetMACAndIR123(regs, 1, (s64)mac2 * scale + (s64)ir0 * ir2,
                      shift, lm);
    gteSetMACAndIR123(regs, 2, (s64)mac3 * scale + (s64)ir0 * ir3,
                      shift, lm);
    gtePushRGBFromMAC(regs);
    gteUpdateErrorFlag(regs);
}

static inline void gteDPCSColor(psxCP2Regs *regs, u8 r, u8 g, u8 b,
                                int shift, int lm) {
    gteSetMAC123(regs, 0, (s64)r * 65536, 0);
    gteSetMAC123(regs, 1, (s64)g * 65536, 0);
    gteSetMAC123(regs, 2, (s64)b * 65536, 0);
    gteInterpolateColor(regs, gteMAC1, gteMAC2, gteMAC3, shift, lm);
    gtePushRGBFromMAC(regs);
}

void gteDPCS(psxCP2Regs *regs) {
    const int shift = GTE_SF(gteop) ? 12 : 0;
    const int lm = GTE_LM(gteop);
    const u8 r = gteR;
    const u8 g = gteG;
    const u8 b = gteB;

#ifdef GTE_LOG
    GTE_LOG("GTE DPCS\n");
#endif
    gteFLAG = 0;
    gteDPCSColor(regs, r, g, b, shift, lm);
    gteUpdateErrorFlag(regs);
}

void gteDPCT(psxCP2Regs *regs) {
    const int shift = GTE_SF(gteop) ? 12 : 0;
    const int lm = GTE_LM(gteop);
    int v;

#ifdef GTE_LOG
    GTE_LOG("GTE DPCT\n");
#endif
    gteFLAG = 0;

    for (v = 0; v < 3; v++)
        gteDPCSColor(regs, gteR0, gteG0, gteB0, shift, lm);
    gteUpdateErrorFlag(regs);
}

void gteNCS(psxCP2Regs *regs) {
    const int shift = GTE_SF(gteop) ? 12 : 0;
    const int lm = GTE_LM(gteop);

#ifdef GTE_LOG
    GTE_LOG("GTE NCS\n");
#endif
    gteFLAG = 0;
    gteNCSVertex(regs, gteVX0, gteVY0, gteVZ0, shift, lm);
    gteUpdateErrorFlag(regs);
}

void gteNCT(psxCP2Regs *regs) {
    const int shift = GTE_SF(gteop) ? 12 : 0;
    const int lm = GTE_LM(gteop);
    int v;

#ifdef GTE_LOG
    GTE_LOG("GTE NCT\n");
#endif
    gteFLAG = 0;

    for (v = 0; v < 3; v++)
        gteNCTVertex(regs, VX(v), VY(v), VZ(v), shift, lm);
    gteUpdateErrorFlag(regs);
}

void gteCC(psxCP2Regs *regs) {
    const int shift = GTE_SF(gteop) ? 12 : 0;
    const int lm = GTE_LM(gteop);

#ifdef GTE_LOG
    GTE_LOG("GTE CC\n");
#endif
    gteFLAG = 0;
    gteApplyColorMatrix(regs, shift, lm);
    gteMultiplyColor(regs, shift, lm);
    gtePushRGBFromMAC(regs);
    gteUpdateErrorFlag(regs);
}

void gteINTPL(psxCP2Regs *regs) {
    const int shift = GTE_SF(gteop) ? 12 : 0;
    const int lm = GTE_LM(gteop);
    const s64 in1 = (s64)gteIR1 * 4096;
    const s64 in2 = (s64)gteIR2 * 4096;
    const s64 in3 = (s64)gteIR3 * 4096;

#ifdef GTE_LOG
    GTE_LOG("GTE INTPL\n");
#endif
    gteFLAG = 0;
    gteInterpolateColor(regs, in1, in2, in3, shift, lm);
    gtePushRGBFromMAC(regs);
    gteUpdateErrorFlag(regs);
}

void gteCDP(psxCP2Regs *regs) {
    const int shift = GTE_SF(gteop) ? 12 : 0;
    const int lm = GTE_LM(gteop);
    s64 products[3];

#ifdef GTE_LOG
    GTE_LOG("GTE CDP\n");
#endif
    gteFLAG = 0;
    gteApplyColorMatrix(regs, shift, lm);
    gteGetColorProducts(regs, products);
    gteInterpolateColor(regs, products[0], products[1], products[2], shift, lm);
    gtePushRGBFromMAC(regs);
    gteUpdateErrorFlag(regs);
}
