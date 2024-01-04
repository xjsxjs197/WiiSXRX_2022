/* General Purpose Registers (GPRs) */

#define    r0    0
#define    r1    1
#define    r2    2
#define    r3    3
#define    r4    4
#define    r5    5
#define    r6    6
#define    r7    7
#define    r8    8
#define    r9    9
#define    r10    10
#define    r11    11
#define    r12    12
#define    r13    13
#define    r14    14
#define    r15    15
#define    r16    16
#define    r17    17
#define    r18    18
#define    r19    19
#define    r20    20
#define    r21    21
#define    r22    22
#define    r23    23
#define    r24    24
#define    r25    25
#define    r26    26
#define    r27    27
#define    r28    28
#define    r29    29
#define    r30    30
#define    r31    31

#define    lr    8

/* Floating Point Registers (FPRs) */

#define	fr0		0
#define	fr1		1
#define	fr2		2
#define	fr3		3
#define	fr4		4
#define	fr5		5
#define	fr6		6
#define	fr7		7
#define	fr8		8
#define	fr9		9
#define	fr10	10
#define	fr11	11
#define	fr12	12
#define	fr13	13
#define	fr14	14
#define	fr15	15
#define	fr16	16
#define	fr17	17
#define	fr18	18
#define	fr19	19
#define	fr20	20
#define	fr21	21
#define	fr22	22
#define	fr23	23
#define	fr24	24
#define	fr25	25
#define	fr26	26
#define	fr27	27
#define	fr28	28
#define	fr29	29
#define	fr30	30
#define	fr31	31

#define fr14ShiftR12 14
#define fr15ShiftR16 15
#define fr16ShiftR5  16
#define fr17ShiftR8  17
#define fr10Kmagic   10

.globl gteDivTst
gteDivTst:
    slwi        r7, r4, 1
    cmpw        r3, r7
    bge         BBGE           // if gteH >= gteSZX * 2
    andi.       r7, r4, 0x8000
    bne         TST_DO16      // if gteSZ3 >= 32768
TST_DO15:
    lwz         r3, 128(r5)          // gteTbl15Addr(r5 + 32 * 4)
    //slwi        r4, r4, 2            // * 4
    //lwzx        r3, 4, r3
    lwz         r3, 4(r3)
    blr
TST_DO16:
    li          r3, 1
    blr
BBGE:
    lis         r3, 2
    blr


// u32 NATIVE_DIVIDE(u16 gteH, u16 gteSZX)
// r6: gteSZX
NATIVE_DIVIDE:
    lhz         r5, 234(r3)          // r5 = gteH   (psxRegs.CP2C.p[26].w.l)
    slwi        r7, r6, 1
    cmpw        r5, r7
    bge         DIVIDE_OVERFLOW      // if gteH >= gteSZX * 2
    andi.       r7, r6, 0x8000
    bne         DIVIDE_DO16          // if gteSZ3 >= 32768
DIVIDE_DO15:
    //slwi        r5, r5, 16
    //divwu       r5, r5, r6
    // (u32)gteH * (*(table15 + gteSZ3)
    slwi        r6, r6, 2            // * 4
    lwz         r7, 0(r4)            // gteTbl15Addr(r4)
    lwzx        r6, r7, r6
    mullw       r5, r5, r6
    addi        r5, r5, 16384        // + (1 << 14)
    srwi        r5, r5, 15           //  >> 15
    b           DIVIDE_OVERFLOW_CHK
DIVIDE_DO16:
    // (u32)gteH * (*(table16 + (gteSZ3 - 32768))
    slwi        r6, r6, 2            // * 4
    lwz         r7, 4(r4)            // gteTbl16Addr(r4 + 4)
    addis       r6, r6, -2           // - 32768 * 4
    lwzx        r6, r7, r6
    mullw       r7, r5, r6
    mulhwu      r6, r5, r6
    li          r5, 0
    ori         r5, r5, 0x8000       // + (1 << 15)
    addc        r5, r7, r5
    srwi        r5, r5, 16           //  >> 16
    addze       r6, r6
    rlwimi      r5, r6, 0x10, 0, 0xf
DIVIDE_OVERFLOW_CHK:
    addis       r6, 0, 2
    subfme      r6, r6
    cmpl        0, 1, r6, r5
    blt         DIVIDE_OVERFLOW      // 0x1ffff < retVal
    xoris       r5, r5, 0x8000
    stw         r5, FloatBufB3@sdarel(r13)
    lfd         fr5, FloatBufA3@sdarel(r13)
    fsub        fr5, fr5, fr10Kmagic // fr5 = retVal(double)
    frsp        fr5, fr5             // fr5 = retVal(single)
    blr

DIVIDE_OVERFLOW:
    lfs         fr5, MAXDIV@sdarel(r13) // fr5 = 0x1ffff
    blr

.globl gtePsRtps
// r3: CP2D(vy0, vx0, 0, vz0, vy1, vx1, 0, vz1, vy2, vx2, 0, vz2)
//     CP2C(m12, m11, m21, m13, m23, m22, m32, m31, 0, m33)(r3 + 128)
// r4: gteTbl15Addr, gteTbl16Addr, gteTmpAddr
gtePsRtps:
    lfs         fr14ShiftR12, UnitShiftR12@sdarel(r13)
    mfspr       r9, lr              // save link register
    ps_merge00  fr14ShiftR12,fr14ShiftR12,fr14ShiftR12  // fr14 = 1 >> 12, 1 >> 12

    psq_l       fr0, 128(r3), 0, 3  // fr0 = m12, m11 GQR3 = 0(s16)
    psq_l       fr6, 0(r3), 0, 3    // fr6 = y, x

    psq_l       fr2, 138(r3), 1, 3  // fr2 = m22
    psq_l       fr9, 132(r3), 1, 3  // fr9 = m21

    ps_mul      fr8, fr0, fr6       // fr8 = m12 * y, m11 * x
    ps_merge00  fr2, fr2, fr9       // fr2 = m22, m21
    psq_l       fr4, 140(r3), 0, 3  // fr4 = m32, m31
    ps_mul      fr10, fr2, fr6      // fr10 = m22 * y, m21 * x
    psq_l       fr7, 6(r3), 1, 3    // fr7 = z, 1
    ps_mul      fr12, fr4, fr6      // fr12 = m32 * y, m31 * x

    psq_l       fr3, 134(r3), 1, 3  // fr3 = m13, 1
    ps_sum0     fr8, fr8, fr8, fr8  // fr8 = m12 * y + m11 * x
    psq_l       fr5, 136(r3), 1, 3  // fr5 = m23, 1
    ps_sum0     fr10,fr10,fr10,fr10 // fr10 = m22 * y + m21 * x
    psq_l       fr1, 146(r3), 1, 3  // fr1 = m33, 1
    ps_sum0     fr12,fr12,fr12,fr12 // fr12 = m32 * y + m31 * x
    ps_madd     fr9,fr3,fr7,fr8     // fr9 = m13 * z + m12 * y + m11 * x  （SUMX）
    ps_madd     fr11,fr5,fr7,fr10   // fr11 = m23 * z + m22 * y + m21 * x （SUMY）
    ps_madd     fr13,fr1,fr7,fr12   // fr13 = m33 * z + m32 * y + m31 * x （SUMZ)

    lfd         fr10Kmagic, kmagic@sdarel(r13)

    // load TRX, TRY, TRZ to float
    lwz         r5, 148(r3)         // gteTRX (((s32 *)regs->CP2C.r)[5])
    lwz         r6, 152(r3)         // gteTRY (((s32 *)regs->CP2C.r)[6])
    lwz         r7, 156(r3)         // gteTRZ (((s32 *)regs->CP2C.r)[7])
    xoris       r5, r5, 0x8000
    xoris       r6, r6, 0x8000
    xoris       r7, r7, 0x8000
    stw         r5, FloatBufB1@sdarel(r13)
    stw         r6, FloatBufB2@sdarel(r13)
    stw         r7, FloatBufB3@sdarel(r13)
    lfd         fr3, FloatBufA1@sdarel(r13)
    lfd         fr4, FloatBufA2@sdarel(r13)
    lfd         fr5, FloatBufA3@sdarel(r13)
    fsub        fr3, fr3, fr10Kmagic        // fr3 = TRX(double)
    fsub        fr4, fr4, fr10Kmagic        // fr4 = TRY(double)
    fsub        fr5, fr5, fr10Kmagic        // fr5 = TRZ(double)
    frsp        fr3, fr3                    // fr3 = TRX(single)
    frsp        fr4, fr4                    // fr4 = TRY(single)
    frsp        fr5, fr5                    // fr5 = TRZ(single)

    ps_merge00  fr9, fr9, fr11              // fr9 = SUMX, SUMY
    ps_merge00  fr3, fr3, fr4               // fr3 = TRX, TRY
    ps_madd     fr2, fr9, fr14ShiftR12, fr3 // fr2 = SUMX >> 12 + TRX, SUMY >> 12 + TRY
    ps_madd     fr5, fr13, fr14ShiftR12, fr5 // fr5 = SUMZ >> 12 + TRZ, 0
    ps_merge10  fr9, fr2, fr2               // fr9 = SUMY >> 12 + TRY

    // Backup MACX, MACY, MACZ
    ps_mr       fr3, fr2
    ps_mr       fr4, fr9
    ps_mr       fr6, fr5

    // Save MACX, MACY to tmpGteAddr
    psq_st      fr2, 8(r4), 0, 3     // save MACX, MACY to tmp gteIR1, gteIR2(r4 + 4 * 2)

    fctiw       fr2, fr2             // MACX => int
    fctiw       fr9, fr9             // MACY => int
    fctiw       fr5, fr5             // MACZ => int

    addi        r5, r3, 100
    addi        r6, r3, 104
    addi        r7, r3, 108

    stfiwx      fr2, 0, r5           // store MACX (((s32 *)regs->CP2D.r)[25])
    stfiwx      fr9, 0, r6           // store MACY (((s32 *)regs->CP2D.r)[26])
    stfiwx      fr5, 0, r7           // store MACZ (((s32 *)regs->CP2D.r)[27])

    // Set gteIR1, gteIR2, gteIR3
    psq_st      fr3, 38(r3), 1, 3    // (regs->CP2D.p[9].sw.l)
    psq_st      fr4, 42(r3), 1, 3    // (regs->CP2D.p[10].sw.l)
    psq_st      fr6, 46(r3), 1, 3    // (regs->CP2D.p[11].sw.l)

    // gteSZ0 = gteSZ1; gteSZ1 = gteSZ2; gteSZ2 = gteSZ3; gteSZ3 = limD(gteMAC3);
    lhz         r5,70(r3)                    // gteSZ1  (regs->CP2D.p[17].w.l)
    lhz         r6,74(r3)                    // gteSZ2  (regs->CP2D.p[18].w.l)
    lhz         r7,78(r3)                    // gteSZ3  (regs->CP2D.p[19].w.l)
    sth         r5,66(r3)                    // gteSZ0  (regs->CP2D.p[16].w.l)
    sth         r6,70(r3)
    sth         r7,74(r3)
    psq_st      fr6, 78(r3), 1, 4            // gteSZ3  (regs->CP2D.p[19].w.l)

    // quotient = limE(DIVIDE_INT(gteH, gteSZ3));
    lhz         r6, 78(r3)                   // r6 = gteSZ3  (regs->CP2D.p[19].w.l)
    bl          NATIVE_DIVIDE                // fr5 = quotient

    // gteSXY0 = gteSXY1;
    // gteSXY1 = gteSXY2;
    lwz         r5,52(r3)                    // gteSXY1 (regs->CP2D.r[13])
    lwz         r6,56(r3)                    // gteSXY2 (regs->CP2D.r[14])
    stw         r5,48(r3)                    // gteSXY0 (regs->CP2D.r[12])
    stw         r6,52(r3)                    // gteSXY1 (regs->CP2D.r[13])

    // (s64)gteOFX + ((s64)gteIR1 * quotient)
    // (s64)gteOFY + ((s64)gteIR2 * quotient)
    lwz         r7, 224(r3)                  // gteOFX (((s32 *)regs->CP2C.r)[24])
    lwz         r6, 228(r3)                  // gteOFY (((s32 *)regs->CP2C.r)[25])
    psq_l       fr6, 8(r4), 0, 3             // load tmp gteIR1, gteIR2(r4 + 4 * 2)
    ps_merge00  fr5, fr5, fr5                // fr5 = quotient, quotient
    lfs         fr15ShiftR16, UnitShiftR16@sdarel(r13)

    xoris       r7, r7, 0x8000
    xoris       r6, r6, 0x8000
    stw         r7, FloatBufB1@sdarel(r13)
    stw         r6, FloatBufB2@sdarel(r13)
    lfd         fr0, FloatBufA1@sdarel(r13)
    lfd         fr1, FloatBufA2@sdarel(r13)
    fsub        fr0, fr0, fr10Kmagic         // fr0 = gteOFX
    fsub        fr1, fr1, fr10Kmagic         // fr1 = gteOFY
    frsp        fr0, fr0                     // double to single
    frsp        fr1, fr1                     // double to single
    ps_merge00  fr0, fr0, fr1                // fr0 = gteOFX, gteOFY
    ps_merge00  fr15ShiftR16, fr15ShiftR16, fr15ShiftR16 // fr15 = 1 >> 16 , 1 >> 16

    ps_madd     fr0, fr6, fr5, fr0           // fr0 = (s64)gteOFX + ((s64)gteIR1 * quotient) / (s64)gteOFY + ((s64)gteIR2 * quotient)
    ps_mul      fr0, fr0, fr15ShiftR16

    // save limG1(gteSX2), limG2(gteSY2)
    psq_st      fr0, 8(r4), 0, 6             // save to tmp gteSX2, gteSY2 (float << 5 => s16)(r4 + 4 * 2)
    psq_l       fr1, 8(r4), 0, 6             // load tmp gteSX2, gteSY2 (s16 >> 5 => float)(r4 + 4 * 2)
    ps_merge10  fr1, fr1, fr1                // fr1 = gteSY2, gteSX2
    psq_st      fr1, 56(r3), 0, 3            // gteSXY2 (regs->CP2D.r[14])

    // (s64)gteDQB + ((s64)gteDQA * quotient)
    lwz         r5, 240(r3)                  // gteDQB (((s32 *)regs->CP2C.r)[28])
    psq_l       fr0, 238(r3), 1, 3           // fr0 = gteDQA (regs->CP2C.p[27].sw.l) / 1.0
    xoris       r5, r5, 0x8000
    stw         r5, FloatBufB1@sdarel(r13)
    lfd         fr1, FloatBufA1@sdarel(r13)
    fsub        fr1, fr1, fr10Kmagic         // fr1 = gteDQB
    frsp        fr1, fr1                     // double to single
    fmadds      fr0, fr0, fr5, fr1           // fr0 = ((s64)gteDQA * quotient) + (s64)gteDQB

    // gteMAC0 = F(tmp);
    fctiw       fr1, fr0                     // fr0 => int
    addi        r5, r3, 96
    stfiwx      fr1, 0, r5                   // store gteMAC0  (((s32 *)regs->CP2D.r)[24])

    // gteIR0 = limH(tmp >> 12)
    fmuls       fr0, fr0, fr14ShiftR12
    psq_st      fr0, 8(r4), 1, 1             // save to tmp gteIR0 (float << 4 => u16)(r4 + 4 * 2)
    psq_l       fr1, 8(r4), 1, 1             // load tmp gteIR0 (u16 >> 4 => float)(r4 + 4 * 2)
    psq_st      fr1, 34(r3), 1, 3            // save to gteIR0  (regs->CP2D.p[8].sw.l)

    mtspr       lr, r9
    blr


.globl gtePsRtpt
// r3: CP2D(vy0, vx0, 0, vz0, vy1, vx1, 0, vz1, vy2, vx2, 0, vz2)
//     CP2C(m12, m11, m21, m13, m23, m22, m32, m31, 0, m33)(r3 + 128)
// r4: gteTbl15Addr, gteTbl16Addr, gteTmpAddr
gtePsRtpt:
    mfspr       r9, lr              // save link register
    lfs         fr14ShiftR12, UnitShiftR12@sdarel(r13)
    addi        r8, r3, 0
    ps_merge00  fr14ShiftR12,fr14ShiftR12,fr14ShiftR12  // fr14 = 1 >> 12, 1 >> 12

    // gteSZ0 = gteSZ3;
    lwz         r5, 76(r3)          // gteSZ3  (regs->CP2D.p[19].w.l)
    li          r10, 0
    stw         r5, 64(r3)          // gteSZ0  (regs->CP2D.p[16].w.l)

rtptVecMulMt:
    psq_l       fr0, 128(r3), 0, 3  // fr0 = m12, m11 GQR3 = 0(s16)
    psq_l       fr6, 0(r8), 0, 3    // fr6 = y, x

    psq_l       fr2, 138(r3), 1, 3  // fr2 = m22
    psq_l       fr9, 132(r3), 1, 3  // fr9 = m21

    ps_mul      fr8, fr0, fr6       // fr8 = m12 * y, m11 * x
    ps_merge00  fr2, fr2, fr9       // fr2 = m22, m21
    psq_l       fr4, 140(r3), 0, 3  // fr4 = m32, m31
    ps_mul      fr10, fr2, fr6      // fr10 = m22 * y, m21 * x
    psq_l       fr7, 6(r8), 1, 3    // fr7 = z, 1
    ps_mul      fr12, fr4, fr6      // fr12 = m32 * y, m31 * x

    psq_l       fr3, 134(r3), 1, 3  // fr3 = m13, 1
    ps_sum0     fr8, fr8, fr8, fr8  // fr8 = m12 * y + m11 * x
    psq_l       fr5, 136(r3), 1, 3  // fr5 = m23, 1
    ps_sum0     fr10,fr10,fr10,fr10 // fr10 = m22 * y + m21 * x
    psq_l       fr1, 146(r3), 1, 3  // fr1 = m33, 1
    ps_sum0     fr12,fr12,fr12,fr12 // fr12 = m32 * y + m31 * x
    ps_madd     fr9,fr3,fr7,fr8     // fr9 = m13 * z + m12 * y + m11 * x  （SUMX）
    ps_madd     fr11,fr5,fr7,fr10   // fr11 = m23 * z + m22 * y + m21 * x （SUMY）
    ps_madd     fr13,fr1,fr7,fr12   // fr13 = m33 * z + m32 * y + m31 * x （SUMZ)

    addi        r8, r8, 8
    lfd         fr10Kmagic, kmagic@sdarel(r13)

    // load TRX, TRY, TRZ to float
    lwz         r5, 148(r3)         // gteTRX (((s32 *)regs->CP2C.r)[5])
    lwz         r6, 152(r3)         // gteTRY (((s32 *)regs->CP2C.r)[6])
    lwz         r7, 156(r3)         // gteTRZ (((s32 *)regs->CP2C.r)[7])
    xoris       r5, r5, 0x8000
    xoris       r6, r6, 0x8000
    xoris       r7, r7, 0x8000
    stw         r5, FloatBufB1@sdarel(r13)
    stw         r6, FloatBufB2@sdarel(r13)
    stw         r7, FloatBufB3@sdarel(r13)
    lfd         fr3, FloatBufA1@sdarel(r13)
    lfd         fr4, FloatBufA2@sdarel(r13)
    lfd         fr5, FloatBufA3@sdarel(r13)
    fsub        fr3, fr3, fr10Kmagic        // fr3 = TRX(double)
    fsub        fr4, fr4, fr10Kmagic        // fr4 = TRY(double)
    fsub        fr5, fr5, fr10Kmagic        // fr5 = TRZ(double)
    frsp        fr3, fr3                    // fr3 = TRX(single)
    frsp        fr4, fr4                    // fr4 = TRY(single)
    frsp        fr5, fr5                    // fr5 = TRZ(single)

    ps_merge00  fr9, fr9, fr11              // fr9 = SUMX, SUMY
    ps_merge00  fr3, fr3, fr4               // fr3 = TRX, TRY
    ps_madd     fr2, fr9, fr14ShiftR12, fr3 // fr2 = SUMX >> 12 + TRX, SUMY >> 12 + TRY
    ps_madd     fr5, fr13, fr14ShiftR12, fr5 // fr5 = SUMZ >> 12 + TRZ, 0
    ps_merge10  fr9, fr2, fr2               // fr9 = SUMY >> 12 + TRY

    // Backup MACX, MACY, MACZ
    ps_mr       fr3, fr2
    ps_mr       fr4, fr9
    ps_mr       fr6, fr5

    // Save MACX, MACY to tmpGteAddr
    psq_st      fr2, 8(r4), 0, 3     // save MACX, MACY to tmp gteIR1, gteIR2(r4 + 4 * 2)

    // only v2 save to MAC
    cmpli       0, 1, r10, 16
    blt         SET_IR
    fctiw       fr2, fr2             // MACX => int
    fctiw       fr9, fr9             // MACY => int
    fctiw       fr5, fr5             // MACZ => int

    addi        r5, r3, 100
    addi        r6, r3, 104
    addi        r7, r3, 108

    stfiwx      fr2, 0, r5           // store MACX (((s32 *)regs->CP2D.r)[25])
    stfiwx      fr9, 0, r6           // store MACY (((s32 *)regs->CP2D.r)[26])
    stfiwx      fr5, 0, r7           // store MACZ (((s32 *)regs->CP2D.r)[27])

SET_IR:
    // Set gteIR1, gteIR2, gteIR3
    psq_st      fr3, 38(r3), 1, 3    // (regs->CP2D.p[9].sw.l)
    psq_st      fr4, 42(r3), 1, 3    // (regs->CP2D.p[10].sw.l)
    psq_st      fr6, 46(r3), 1, 3    // (regs->CP2D.p[11].sw.l)

    // gteSZX = limD(gteMAC3);
    srwi        r5, r10, 1
    addi        r5, r5, 70
    psq_stx     fr6, r5, r3, 1, 4            // gteSZX  (regs->CP2D.p[17, 18, 19].w.l)

    // quotient = limE(DIVIDE_INT(gteH, gteSZX));
    lhzx        r6, r5, r3                   // r6 = gteSZX  (regs->CP2D.p[17, 18, 19].w.l)
    bl          NATIVE_DIVIDE                // fr5 = quotient

    // (s64)gteOFX + ((s64)gteIR1 * quotient)
    // (s64)gteOFY + ((s64)gteIR2 * quotient)
    lwz         r7, 224(r3)                  // gteOFX (((s32 *)regs->CP2C.r)[24])
    lwz         r6, 228(r3)                  // gteOFY (((s32 *)regs->CP2C.r)[25])
    psq_l       fr6, 8(r4), 0, 3             // load tmp gteIR1, gteIR2(r4 + 4 * 2)
    ps_merge00  fr5, fr5, fr5                // fr5 = quotient, quotient
    lfs         fr15ShiftR16, UnitShiftR16@sdarel(r13)

    xoris       r7, r7, 0x8000
    xoris       r6, r6, 0x8000
    stw         r7, FloatBufB1@sdarel(r13)
    stw         r6, FloatBufB2@sdarel(r13)
    lfd         fr0, FloatBufA1@sdarel(r13)
    lfd         fr1, FloatBufA2@sdarel(r13)
    fsub        fr0, fr0, fr10Kmagic         // fr0 = gteOFX
    fsub        fr1, fr1, fr10Kmagic         // fr1 = gteOFY
    frsp        fr0, fr0                     // double to single
    frsp        fr1, fr1                     // double to single
    ps_merge00  fr0, fr0, fr1                // fr0 = gteOFX, gteOFY
    ps_merge00  fr15ShiftR16, fr15ShiftR16, fr15ShiftR16 // fr15 = 1 >> 16 , 1 >> 16

    ps_madd     fr0, fr6, fr5, fr0           // fr0 = (s64)gteOFX + ((s64)gteIR1 * quotient) / (s64)gteOFY + ((s64)gteIR2 * quotient)
    ps_mul      fr0, fr0, fr15ShiftR16

    // save limG1(gteSXN), limG2(gteSYN)
    srwi        r5, r10, 1
    psq_st      fr0, 8(r4), 0, 6             // save to tmp gteSXN, gteSYN (float << 5 => s16)(r4 + 4 * 2)
    psq_l       fr1, 8(r4), 0, 6             // load tmp gteSXN, gteSYN (s16 >> 5 => float)(r4 + 4 * 2)
    addi        r5, r5, 48
    ps_merge10  fr1, fr1, fr1                // fr1 = gteSYN, gteSXN
    psq_stx     fr1, r5, r3, 0, 3            // gteSXYN (regs->CP2D.r[12, 13, 14])

    // loop
    addi        r10, r10, 8
    cmpli       0, 1, r10, 24
    blt         rtptVecMulMt                 // loop v0, v1, v2

    // (s64)gteDQB + ((s64)gteDQA * quotient)
    lwz         r5, 240(r3)                  // gteDQB (((s32 *)regs->CP2C.r)[28])
    psq_l       fr0, 238(r3), 1, 3           // fr0 = gteDQA (regs->CP2C.p[27].sw.l) / 1.0
    xoris       r5, r5, 0x8000
    stw         r5, FloatBufB1@sdarel(r13)
    lfd         fr1, FloatBufA1@sdarel(r13)
    fsub        fr1, fr1, fr10Kmagic         // fr1 = gteDQB
    frsp        fr1, fr1                     // double to single
    fmadds      fr0, fr0, fr5, fr1           // fr0 = ((s64)gteDQA * quotient) + (s64)gteDQB

    // gteMAC0 = F(tmp);
    fctiw       fr1, fr0                     // fr0 => int
    addi        r5, r3, 96
    stfiwx      fr1, 0, r5                   // store gteMAC0  (((s32 *)regs->CP2D.r)[24])

    // gteIR0 = limH(tmp >> 12)
    fmuls       fr0, fr0, fr14ShiftR12
    psq_st      fr0, 8(r4), 1, 1             // save to tmp gteIR0 (float << 4 => u16)(r4 + 4 * 2)
    psq_l       fr1, 8(r4), 1, 1             // load tmp gteIR0 (u16 >> 4 => float)(r4 + 4 * 2)
    psq_st      fr1, 34(r3), 1, 3            // save to gteIR0  (regs->CP2D.p[8].sw.l)

    mtspr       lr, r9
    blr


.globl gtePsNccs
// r3: CP2D(vy0, vx0, 0, vz0, vy1, vx1, 0, vz1, vy2, vx2, 0, vz2)
//     CP2C(m12, m11, m21, m13, m23, m22, m32, m31, 0, m33)(r3 + 128)
//     CP2C(L12, L11, L21, L13, L23, L22, L32, L31, 0, L33)(r3 + 128 + 8 * 4)
//     CP2C(LR2, LR1, LG1, LR3, LG3, LG2, LB2, LB1, 0, LB3)(r3 + 128 + 16 * 4)
// r4: gteTbl15Addr, gteTbl16Addr, gteTmpAddr
gtePsNccs:
    lfs         fr14ShiftR12, UnitShiftR12@sdarel(r13)
    lfs         fr17ShiftR8, UnitShiftR8@sdarel(r13)
    mfspr       r9, lr              // save link register
    ps_merge00  fr14ShiftR12,fr14ShiftR12,fr14ShiftR12  // fr14 = 1 >> 12, 1 >> 12
    ps_merge00  fr17ShiftR8, fr17ShiftR8, fr17ShiftR8   // fr17 = 1 >> 8, 1 >> 8

    psq_l       fr0, 160(r3), 0, 3  // fr0 = L12, L11 GQR3 = 0(s16)
    psq_l       fr6, 0(r3), 0, 3    // fr6 = y, x

    psq_l       fr2, 170(r3), 1, 3  // fr2 = L22
    psq_l       fr9, 164(r3), 1, 3  // fr9 = L21

    ps_mul      fr8, fr0, fr6       // fr8 = L12 * y, L11 * x
    ps_merge00  fr2, fr2, fr9       // fr2 = L22, L21
    psq_l       fr4, 172(r3), 0, 3  // fr4 = L32, L31
    ps_mul      fr10, fr2, fr6      // fr10 = L22 * y, L* x
    psq_l       fr7, 6(r3), 1, 3    // fr7 = z, 1
    ps_mul      fr12, fr4, fr6      // fr12 = L32 * y, L31 * x

    psq_l       fr3, 166(r3), 1, 3  // fr3 = L13, 1
    ps_sum0     fr8, fr8, fr8, fr8  // fr8 = L12 * y + L11 * x
    psq_l       fr5, 168(r3), 1, 3  // fr5 = L23, 1
    ps_sum0     fr10,fr10,fr10,fr10 // fr10 = L22 * y + L21 * x
    psq_l       fr1, 178(r3), 1, 3  // fr1 = L33, 1
    ps_sum0     fr12,fr12,fr12,fr12 // fr12 = L32 * y + L31 * x
    ps_madd     fr9,fr3,fr7,fr8     // fr9 = L13 * z + L12 * y + L11 * x  （SUMX）
    ps_madd     fr11,fr5,fr7,fr10   // fr11 = L23 * z + L22 * y + L21 * x （SUMY）
    ps_madd     fr13,fr1,fr7,fr12   // fr13 = L33 * z + L32 * y + L31 * x （SUMZ)

    ps_merge00  fr9, fr11, fr9              // fr9 = SUMY, SUMX
    ps_mul      fr5, fr13, fr14ShiftR12     // fr5 = SUMZ >> 12, 0
    ps_mul      fr2, fr9, fr14ShiftR12      // fr2 = SUMY >> 12, SUMX >> 12

    lfd         fr10Kmagic, kmagic@sdarel(r13)

    // Save MACX, MACY, MACZ to tmpGteAddr
    psq_st      fr5, 12(r4), 1, 7    // save MACZ to tmp gteIR3(r4 + 4 * 3)
    psq_st      fr2, 8(r4), 0, 7     // save MACY, MACX to tmp gteIR2, gteIR1(r4 + 4 * 2)

    // LR * IR
    psq_l       fr0, 192(r3), 0, 3  // fr0 = LR2, LR1 GQR3 = 0(s16)
    psq_l       fr6, 8(r4), 0, 7    // fr6 = y, x

    psq_l       fr2, 202(r3), 1, 3  // fr2 = LG2
    psq_l       fr9, 196(r3), 1, 3  // fr9 = LG1

    ps_mul      fr8, fr0, fr6       // fr8 = LR2 * y, LR1 * x
    ps_merge00  fr2, fr2, fr9       // fr2 = LG2, LG1
    psq_l       fr4, 204(r3), 0, 3  // fr4 = LB2, LB1
    ps_mul      fr10, fr2, fr6      // fr10 = LG2 * y, LG2* x
    psq_l       fr7, 12(r4), 1, 7   // fr7 = z, 1
    ps_mul      fr12, fr4, fr6      // fr12 = LB2 * y, LB1 * x

    psq_l       fr3, 198(r3), 1, 3  // fr3 = LR3, 1
    ps_sum0     fr8, fr8, fr8, fr8  // fr8 = LR2 * y + LR1 * x
    psq_l       fr5, 200(r3), 1, 3  // fr5 = LG3, 1
    ps_sum0     fr10,fr10,fr10,fr10 // fr10 = LG2 * y + LG1 * x
    psq_l       fr1, 210(r3), 1, 3  // fr1 = LB3, 1
    ps_sum0     fr12,fr12,fr12,fr12 // fr12 = LB2 * y + LB1 * x
    ps_madd     fr9,fr3,fr7,fr8     // fr9 = LR3 * z + LR2 * y + LR1 * x  （SUMX）
    ps_madd     fr11,fr5,fr7,fr10   // fr11 = LG3 * z + LG2 * y + LG1 * x （SUMY）
    ps_madd     fr13,fr1,fr7,fr12   // fr13 = LB3 * z + LB2 * y + LB1 * x （SUMZ)

    ps_merge00  fr9, fr11, fr9      // fr9 = SUMY, SUMX

    // load gteRBK, gteGBK, gteBBK to float
    lwz         r5, 180(r3)         // gteRBK (((s32 *)regs->CP2C.r)[13])
    lwz         r6, 184(r3)         // gteGBK (((s32 *)regs->CP2C.r)[14])
    lwz         r7, 188(r3)         // gteBBK (((s32 *)regs->CP2C.r)[15])
    xoris       r5, r5, 0x8000
    xoris       r6, r6, 0x8000
    xoris       r7, r7, 0x8000
    stw         r5, FloatBufB1@sdarel(r13)
    stw         r6, FloatBufB2@sdarel(r13)
    stw         r7, FloatBufB3@sdarel(r13)
    lfd         fr3, FloatBufA1@sdarel(r13)
    lfd         fr4, FloatBufA2@sdarel(r13)
    lfd         fr5, FloatBufA3@sdarel(r13)
    fsub        fr3, fr3, fr10Kmagic        // fr3 = RBK(double)
    fsub        fr4, fr4, fr10Kmagic        // fr4 = GBK(double)
    fsub        fr5, fr5, fr10Kmagic        // fr5 = BBK(double)
    frsp        fr3, fr3                    // fr3 = RBK(single)
    frsp        fr4, fr4                    // fr4 = GBK(single)
    frsp        fr5, fr5                    // fr5 = BBK(single)

    ps_merge00  fr3, fr4, fr3               // fr3 = GBK, RBK
    ps_madd     fr5, fr13, fr14ShiftR12, fr5 // fr5 = SUMZ >> 12 + BBK, 0
    ps_madd     fr2, fr9, fr14ShiftR12, fr3 // fr2 = SUMY >> 12 + GBK, SUMX >> 12 + RBK

    // Save MACX, MACY, MACZ to tmpGteAddr
    psq_st      fr5, 12(r4), 1, 7    // save MACZ to tmp gteIR3(r4 + 4 * 3)
    psq_st      fr2, 8(r4), 0, 7     // save MACY, MACX to tmp gteIR2, gteIR1(r4 + 4 * 2)

    // RGB * IR (gteG (regs->CP2D.p[6].b.h))
    psq_l       fr0, 26(r3), 0, 2    // fr0 = gteG, gteR GQR2 = 0(u8)
    psq_l       fr3, 25(r3), 1, 2    // fr3 = gteB, 1
    psq_l       fr5, 12(r4), 1, 7    // load tmp gteIR3(r4 + 4 * 3)
    psq_l       fr9, 8(r4), 0, 7     // load tmp gteIR2, gteIR1(r4 + 4 * 2)
    ps_mul      fr5, fr3, fr5        // fr5 = gteB * gteIR3
    ps_mul      fr9, fr0, fr9        // fr5 = gteG * gteIR2, gteR * gteIR1

    // gteIR1 = gteMAC1 = ((s32)gteR * gteIR1) >> 8
    // gteIR2 = gteMAC2 = ((s32)gteG * gteIR2) >> 8
    // gteIR3 = gteMAC3 = ((s32)gteB * gteIR3) >> 8
    ps_mul      fr3, fr5, fr17ShiftR8 // fr3 = gteMAC3 >> 8
    ps_mul      fr4, fr9, fr17ShiftR8 // fr4 = gteMAC2 >> 8, gteMAC1 >> 8

    psq_st      fr3, 46(r3), 1, 3    // gteIR3 (regs->CP2D.p[11].sw.l)
    psq_st      fr4, 42(r3), 1, 3    // gteIR2 (regs->CP2D.p[10].sw.l)
    ps_merge10  fr4, fr4, fr4        // fr4 = gteMAC1 >> 8, gteMAC2 >> 8
    psq_st      fr4, 38(r3), 1, 3    // gteIR1 (regs->CP2D.p[9].sw.l)

    // gteRGB0 = gteRGB1; gteRGB1 = gteRGB2; gteCODE2 = gteCODE;
    lwz         r5, 84(r3)           // gteRGB1  (regs->CP2D.r[21])
    lwz         r6, 88(r3)           // gteRGB2  (regs->CP2D.r[22])
    lbz         r7, 24(r3)           // gteCODE (regs->CP2D.p[6].b.h3)
    stw         r5, 80(r3)           // gteRGB0  (regs->CP2D.r[20])
    stw         r6, 84(r3)           // gteRGB1  (regs->CP2D.r[21])
    stb         r7, 88(r3)           // gteCODE2 (regs->CP2D.p[22].b.h3)

    // gteR2 = limC1(((s32)gteR * gteIR1) >> 8 >> 4);
    // gteG2 = limC2(((s32)gteG * gteIR2) >> 8 >> 4);
    // gteB2 = limC3(((s32)gteB * gteIR3) >> 8 >> 4);
    //ps_mul      fr3, fr5, fr14ShiftR12 // fr3 = gteMAC3 >> 12
    //ps_mul      fr4, fr9, fr14ShiftR12 // fr4 = gteMAC2 >> 12, gteMAC1 >> 12
    psq_st      fr5, 89(r3), 1, 5      // gteB2    (regs->CP2D.p[22].b.h2)
    psq_st      fr9, 90(r3), 0, 5      // gteG2    (regs->CP2D.p[22].b.h), gteR2    (regs->CP2D.p[22].b.l)

    mtspr       lr, r9
    blr


.globl gtePsNcct
// r3: CP2D(vy0, vx0, 0, vz0, vy1, vx1, 0, vz1, vy2, vx2, 0, vz2)
//     CP2C(m12, m11, m21, m13, m23, m22, m32, m31, 0, m33)(r3 + 128)
//     CP2C(L12, L11, L21, L13, L23, L22, L32, L31, 0, L33)(r3 + 128 + 8 * 4)
//     CP2C(LR2, LR1, LG1, LR3, LG3, LG2, LB2, LB1, 0, LB3)(r3 + 128 + 16 * 4)
// r4: gteTbl15Addr, gteTbl16Addr, gteTmpAddr
gtePsNcct:
    lfs         fr14ShiftR12, UnitShiftR12@sdarel(r13)
    lfs         fr17ShiftR8, UnitShiftR8@sdarel(r13)
    mfspr       r9, lr              // save link register
    addi        r8, r3, 0
    li          r10, 0
    ps_merge00  fr14ShiftR12,fr14ShiftR12,fr14ShiftR12  // fr14 = 1 >> 12, 1 >> 12
    ps_merge00  fr17ShiftR8, fr17ShiftR8, fr17ShiftR8   // fr17 = 1 >> 8, 1 >> 8

ncctVecMulMt:
    psq_l       fr0, 160(r3), 0, 3  // fr0 = L12, L11 GQR3 = 0(s16)
    psq_l       fr6, 0(r8), 0, 3    // fr6 = y, x

    psq_l       fr2, 170(r3), 1, 3  // fr2 = L22
    psq_l       fr9, 164(r3), 1, 3  // fr9 = L21

    ps_mul      fr8, fr0, fr6       // fr8 = L12 * y, L11 * x
    ps_merge00  fr2, fr2, fr9       // fr2 = L22, L21
    psq_l       fr4, 172(r3), 0, 3  // fr4 = L32, L31
    ps_mul      fr10, fr2, fr6      // fr10 = L22 * y, L* x
    psq_l       fr7, 6(r8), 1, 3    // fr7 = z, 1
    ps_mul      fr12, fr4, fr6      // fr12 = L32 * y, L31 * x

    psq_l       fr3, 166(r3), 1, 3  // fr3 = L13, 1
    ps_sum0     fr8, fr8, fr8, fr8  // fr8 = L12 * y + L11 * x
    psq_l       fr5, 168(r3), 1, 3  // fr5 = L23, 1
    ps_sum0     fr10,fr10,fr10,fr10 // fr10 = L22 * y + L21 * x
    psq_l       fr1, 178(r3), 1, 3  // fr1 = L33, 1
    ps_sum0     fr12,fr12,fr12,fr12 // fr12 = L32 * y + L31 * x
    ps_madd     fr9,fr3,fr7,fr8     // fr9 = L13 * z + L12 * y + L11 * x  （SUMX）
    ps_madd     fr11,fr5,fr7,fr10   // fr11 = L23 * z + L22 * y + L21 * x （SUMY）
    ps_madd     fr13,fr1,fr7,fr12   // fr13 = L33 * z + L32 * y + L31 * x （SUMZ)

    ps_merge00  fr9, fr11, fr9              // fr9 = SUMY, SUMX
    ps_mul      fr5, fr13, fr14ShiftR12     // fr5 = SUMZ >> 12, 0
    ps_mul      fr2, fr9, fr14ShiftR12      // fr2 = SUMY >> 12, SUMX >> 12

    addi        r8, r8, 8
    lfd         fr10Kmagic, kmagic@sdarel(r13)

    // Save MACX, MACY, MACZ to tmpGteAddr
    psq_st      fr5, 12(r4), 1, 7    // save MACZ to tmp gteIR3(r4 + 4 * 3)
    psq_st      fr2, 8(r4), 0, 7     // save MACY, MACX to tmp gteIR2, gteIR1(r4 + 4 * 2)

    // LR * IR
    psq_l       fr0, 192(r3), 0, 3  // fr0 = LR2, LR1 GQR3 = 0(s16)
    psq_l       fr6, 8(r4), 0, 7    // fr6 = y, x

    psq_l       fr2, 202(r3), 1, 3  // fr2 = LG2
    psq_l       fr9, 196(r3), 1, 3  // fr9 = LG1

    ps_mul      fr8, fr0, fr6       // fr8 = LR2 * y, LR1 * x
    ps_merge00  fr2, fr2, fr9       // fr2 = LG2, LG1
    psq_l       fr4, 204(r3), 0, 3  // fr4 = LB2, LB1
    ps_mul      fr10, fr2, fr6      // fr10 = LG2 * y, LG2* x
    psq_l       fr7, 12(r4), 1, 7   // fr7 = z, 1
    ps_mul      fr12, fr4, fr6      // fr12 = LB2 * y, LB1 * x

    psq_l       fr3, 198(r3), 1, 3  // fr3 = LR3, 1
    ps_sum0     fr8, fr8, fr8, fr8  // fr8 = LR2 * y + LR1 * x
    psq_l       fr5, 200(r3), 1, 3  // fr5 = LG3, 1
    ps_sum0     fr10,fr10,fr10,fr10 // fr10 = LG2 * y + LG1 * x
    psq_l       fr1, 210(r3), 1, 3  // fr1 = LB3, 1
    ps_sum0     fr12,fr12,fr12,fr12 // fr12 = LB2 * y + LB1 * x
    ps_madd     fr9,fr3,fr7,fr8     // fr9 = LR3 * z + LR2 * y + LR1 * x  （SUMX）
    ps_madd     fr11,fr5,fr7,fr10   // fr11 = LG3 * z + LG2 * y + LG1 * x （SUMY）
    ps_madd     fr13,fr1,fr7,fr12   // fr13 = LB3 * z + LB2 * y + LB1 * x （SUMZ)

    ps_merge00  fr9, fr11, fr9      // fr9 = SUMY, SUMX

    // load gteRBK, gteGBK, gteBBK to float
    lwz         r5, 180(r3)         // gteRBK (((s32 *)regs->CP2C.r)[13])
    lwz         r6, 184(r3)         // gteGBK (((s32 *)regs->CP2C.r)[14])
    lwz         r7, 188(r3)         // gteBBK (((s32 *)regs->CP2C.r)[15])
    xoris       r5, r5, 0x8000
    xoris       r6, r6, 0x8000
    xoris       r7, r7, 0x8000
    stw         r5, FloatBufB1@sdarel(r13)
    stw         r6, FloatBufB2@sdarel(r13)
    stw         r7, FloatBufB3@sdarel(r13)
    lfd         fr3, FloatBufA1@sdarel(r13)
    lfd         fr4, FloatBufA2@sdarel(r13)
    lfd         fr5, FloatBufA3@sdarel(r13)
    fsub        fr3, fr3, fr10Kmagic        // fr3 = RBK(double)
    fsub        fr4, fr4, fr10Kmagic        // fr4 = GBK(double)
    fsub        fr5, fr5, fr10Kmagic        // fr5 = BBK(double)
    frsp        fr3, fr3                    // fr3 = RBK(single)
    frsp        fr4, fr4                    // fr4 = GBK(single)
    frsp        fr5, fr5                    // fr5 = BBK(single)

    ps_merge00  fr3, fr4, fr3               // fr3 = GBK, RBK
    ps_madd     fr5, fr13, fr14ShiftR12, fr5 // fr5 = SUMZ >> 12 + BBK, 0
    ps_madd     fr2, fr9, fr14ShiftR12, fr3 // fr2 = SUMY >> 12 + GBK, SUMX >> 12 + RBK

    // Save MACX, MACY, MACZ to tmpGteAddr
    psq_st      fr5, 12(r4), 1, 7    // save MACZ to tmp gteIR3(r4 + 4 * 3)
    psq_st      fr2, 8(r4), 0, 7     // save MACY, MACX to tmp gteIR2, gteIR1(r4 + 4 * 2)

    // RGB * IR (gteG (regs->CP2D.p[6].b.h))
    psq_l       fr0, 26(r3), 0, 2    // fr0 = gteG, gteR GQR2 = 0(u8)
    psq_l       fr3, 25(r3), 1, 2    // fr3 = gteB, 1
    psq_l       fr5, 12(r4), 1, 7    // load tmp gteIR3(r4 + 4 * 3)
    psq_l       fr9, 8(r4), 0, 7     // load tmp gteIR2, gteIR1(r4 + 4 * 2)
    ps_mul      fr5, fr3, fr5        // fr5 = gteB * gteIR3
    ps_mul      fr9, fr0, fr9        // fr5 = gteG * gteIR2, gteR * gteIR1

    // gteIR1 = gteMAC1 = ((s32)gteR * gteIR1) >> 8
    // gteIR2 = gteMAC2 = ((s32)gteG * gteIR2) >> 8
    // gteIR3 = gteMAC3 = ((s32)gteB * gteIR3) >> 8
    ps_mul      fr3, fr5, fr17ShiftR8 // fr3 = gteMAC3 >> 8
    ps_mul      fr4, fr9, fr17ShiftR8 // fr4 = gteMAC2 >> 8, gteMAC1 >> 8

    psq_st      fr3, 46(r3), 1, 3    // gteIR3 (regs->CP2D.p[11].sw.l)
    psq_st      fr4, 42(r3), 1, 3    // gteIR2 (regs->CP2D.p[10].sw.l)
    ps_merge10  fr4, fr4, fr4        // fr4 = gteMAC1 >> 8, gteMAC2 >> 8
    psq_st      fr4, 38(r3), 1, 3    // gteIR1 (regs->CP2D.p[9].sw.l)

    // gteRGB0 = gteRGB1; gteRGB1 = gteRGB2; gteCODE2 = gteCODE;
    lwz         r5, 84(r3)           // gteRGB1  (regs->CP2D.r[21])
    lwz         r6, 88(r3)           // gteRGB2  (regs->CP2D.r[22])
    lbz         r7, 24(r3)           // gteCODE (regs->CP2D.p[6].b.h3)
    stw         r5, 80(r3)           // gteRGB0  (regs->CP2D.r[20])
    stw         r6, 84(r3)           // gteRGB1  (regs->CP2D.r[21])
    stb         r7, 88(r3)           // gteCODE2 (regs->CP2D.p[22].b.h3)

    // gteR2 = limC1(((s32)gteR * gteIR1) >> 8 >> 4);
    // gteG2 = limC2(((s32)gteG * gteIR2) >> 8 >> 4);
    // gteB2 = limC3(((s32)gteB * gteIR3) >> 8 >> 4);
    //ps_mul      fr3, fr5, fr14ShiftR12 // fr3 = gteMAC3 >> 12
    //ps_mul      fr4, fr9, fr14ShiftR12 // fr4 = gteMAC2 >> 12, gteMAC1 >> 12
    psq_st      fr5, 89(r3), 1, 5      // gteB2    (regs->CP2D.p[22].b.h2)
    psq_st      fr9, 90(r3), 0, 5      // gteG2    (regs->CP2D.p[22].b.h), gteR2    (regs->CP2D.p[22].b.l)

    // loop
    addi        r10, r10, 8
    cmpli       0, 1, r10, 24
    blt         ncctVecMulMt                 // loop v0, v1, v2

    mtspr       lr, r9
    blr


.section .sdata
	.balign 32
MUL2:
	.float 2.0
MUL16:
	.float 65536.0
MAXDIV:
    .float 131071.0                  // 0x1ffff
FloatBufA1:
	.int 0x43300000
FloatBufB1:
	.int 0x80000000
FloatBufA2:
	.int 0x43300000
FloatBufB2:
	.int 0x80000000
FloatBufA3:
	.int 0x43300000
FloatBufB3:
	.int 0x80000000
kmagic:
    .int 0x43300000, 0x80000000
UnitShiftL12:
	.float	4096.0                   // 1 << 12 = 4096.0
UnitShiftR5:
	.float	0.03125                  // 1 >> 5 = 1 / 32 = 0.03125
UnitShiftR8:
	.float	0.00390625               // 1 >> 8 = 1 / 256 = 0.00390625
UnitShiftR12:
	.float	0.000244140625           // 1 >> 12 = 1 / 4096 = 0.000244140625
FncOverflowH:
	.float	2147483647.0
FncOverflowL:
	.float	-2147483648.0
IntOverflowH:
	.int	2147483647
IntOverflowL:
	.int	-2147483648
UnitShiftR16:
	.float	0.00001526               // 1 >> 16 = 1 / 65536 = 0.00001526
debugArea:
    .int 0
TestOverflowH:
	.float	2147483647.0
DivOverflow:
	.int	0x1ffff

