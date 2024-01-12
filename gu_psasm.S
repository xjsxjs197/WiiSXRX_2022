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
#define fr17ShiftR8  17
#define fr18Kmagic   18


// u32 NATIVE_DIVIDE(u16 gteH, u16 gteSZX)
// r6: gteSZX
NATIVE_DIVIDE:
    lhz         r5, 234(r3)          // r5 = gteH   (psxRegs.CP2C.p[26].w.l)
    slwi        r7, r6, 1
    cmpl        3, 0, r5, r7
    bge         3, DIVIDE_OVERFLOW   // if gteH >= gteSZX * 2
    andi.       r7, r6, 0x8000
    bne         0, DIVIDE_DO16       // if gteSZ3 >= 32768
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
    cmpl        3, 0, r6, r5
    blt         3, DIVIDE_OVERFLOW   // 0x1ffff < retVal
    xoris       r5, r5, 0x8000
    stw         r5, FloatBufB3@sdarel(r13)
    lfd         fr5, FloatBufA3@sdarel(r13)
    fsub        fr5, fr5, fr18Kmagic // fr5 = retVal(double)
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
    mfspr       r9, lr              // save link register
    lfs         fr14ShiftR12, UnitShiftR12@sdarel(r13)  // fr14 = 1 >> 12, 1 >> 12
    lfd         fr18Kmagic, kmagic@sdarel(r13)

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
    fmadds      fr9,fr3,fr7,fr8     // fr9 = m13 * z + m12 * y + m11 * x  (SUMX)
    fmadds      fr11,fr5,fr7,fr10   // fr11 = m23 * z + m22 * y + m21 * x (SUMY)
    fmadds      fr13,fr1,fr7,fr12   // fr13 = m33 * z + m32 * y + m31 * x (SUMZ)

    // load TRX, TRY, TRZ to float
    lfs         fr3, 40(r4)         // gteTRX (r4 + 10 * 4)
    lfs         fr4, 44(r4)         // gteTRY (r4 + 11 * 4)
    lfs         fr5, 48(r4)         // gteTRZ (r4 + 12 * 4)

    ps_merge00  fr9, fr9, fr11              // fr9 = SUMX, SUMY
    ps_merge00  fr3, fr3, fr4               // fr3 = TRX, TRY
    ps_madd     fr2, fr9, fr14ShiftR12, fr3 // fr2 = SUMX >> 12 + TRX, SUMY >> 12 + TRY
    fmadds      fr5, fr13, fr14ShiftR12,fr5 // fr5 = SUMZ >> 12 + TRZ, 0
    ps_merge10  fr9, fr2, fr2               // fr9 = SUMY >> 12 + TRY

    fctiw       fr6, fr5             // MACZ => int
    fctiw       fr3, fr2             // MACX => int
    fctiw       fr4, fr9             // MACY => int

    // Save MACX, MACY to tmpGteAddr
    psq_st      fr9, 8(r4), 0, 3     // save MACY, MACX to tmp gteIR2, gteIR1(r4 + 4 * 2)

    addi        r5, r3, 100
    addi        r6, r3, 104
    addi        r7, r3, 108

    stfiwx      fr3, 0, r5           // store MACX (((s32 *)regs->CP2D.r)[25])
    stfiwx      fr4, 0, r6           // store MACY (((s32 *)regs->CP2D.r)[26])
    stfiwx      fr6, 0, r7           // store MACZ (((s32 *)regs->CP2D.r)[27])

    // Set gteIR1, gteIR2, gteIR3
    psq_st      fr2, 38(r3), 1, 3    // (regs->CP2D.p[9].sw.l)
    psq_st      fr9, 42(r3), 1, 3    // (regs->CP2D.p[10].sw.l)
    psq_st      fr5, 46(r3), 1, 3    // (regs->CP2D.p[11].sw.l)

    // gteSZ0 = gteSZ1; gteSZ1 = gteSZ2; gteSZ2 = gteSZ3; gteSZ3 = limD(gteMAC3);
    lhz         r5,70(r3)                    // gteSZ1  (regs->CP2D.p[17].w.l)
    lhz         r6,74(r3)                    // gteSZ2  (regs->CP2D.p[18].w.l)
    lhz         r7,78(r3)                    // gteSZ3  (regs->CP2D.p[19].w.l)
    psq_st      fr5, 78(r3), 1, 4            // gteSZ3  (regs->CP2D.p[19].w.l)

    sth         r5,66(r3)                    // gteSZ0  (regs->CP2D.p[16].w.l)
    sth         r6,70(r3)                    // gteSZ1  (regs->CP2D.p[17].w.l)
    sth         r7,74(r3)                    // gteSZ2  (regs->CP2D.p[18].w.l)

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
    ps_merge00  fr5, fr5, fr5
    lfs         fr0, 52(r4)                  // gteOFX (r4 + 13 * 4)
    lfs         fr1, 56(r4)                  // gteOFY (r4 + 14 * 4)

    psq_l       fr6, 8(r4), 0, 3             // load tmp gteIR2, gteIR1(r4 + 4 * 2)
    ps_merge00  fr0, fr1, fr0                // fr0 = gteOFY, gteOFX

    ps_madd     fr0, fr6, fr5, fr0           // fr0 = (s64)gteOFY + ((s64)gteIR2 * quotient) / (s64)gteOFX + ((s64)gteIR1 * quotient)

    // save limG1(gteSX2), limG2(gteSY2)
    psq_st      fr0, 56(r3), 0, 6            // gteSXY2 (regs->CP2D.r[14])

    // (s64)gteDQB + ((s64)gteDQA * quotient)
    lfs         fr1, 72(r4)                  // gteDQB (r4 + 18 * 4)
    psq_l       fr0, 238(r3), 1, 3           // fr0 = gteDQA (regs->CP2C.p[27].sw.l) / 1.0
    fmadds      fr0, fr0, fr5, fr1           // fr0 = ((s64)gteDQA * quotient) + (s64)gteDQB

    addi        r5, r3, 96

    // gteIR0 = limH(tmp >> 12)
    psq_st      fr0, 34(r3), 1, 1            // gteIR0 (regs->CP2D.p[8].sw.l)

    // gteMAC0 = F(tmp);
    fctiw       fr1, fr0                     // fr0 => int
    stfiwx      fr1, 0, r5                   // store gteMAC0  (((s32 *)regs->CP2D.r)[24])
    lhz         r6, 34(r3)
    li          r7, 0x1000
    cmpli       3, 0, r6, 0x1000
    blt         3, rtpsEnd
    sth         r7, 34(r3)
rtpsEnd:
    mtspr       lr, r9
    blr


.globl gtePsRtpt
// r3: CP2D(vy0, vx0, 0, vz0, vy1, vx1, 0, vz1, vy2, vx2, 0, vz2)
//     CP2C(m12, m11, m21, m13, m23, m22, m32, m31, 0, m33)(r3 + 128)
// r4: gteTbl15Addr, gteTbl16Addr, gteTmpAddr
gtePsRtpt:
    lfs         fr14ShiftR12, UnitShiftR12@sdarel(r13)  // fr14 = 1 >> 12, 1 >> 12
    lfd         fr18Kmagic, kmagic@sdarel(r13)

    // gteSZ0 = gteSZ3;
    lwz         r5, 76(r3)          // gteSZ3  (regs->CP2D.p[19].w.l)
    addi        r8, r3, 0
    mfspr       r9, lr              // save link register
    stw         r5, 64(r3)          // gteSZ0  (regs->CP2D.p[16].w.l)

    // V0
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

    addi        r8, r8, 8
    fmadds      fr9,fr3,fr7,fr8     // fr9 = m13 * z + m12 * y + m11 * x  (SUMX)
    fmadds      fr11,fr5,fr7,fr10   // fr11 = m23 * z + m22 * y + m21 * x (SUMY)
    fmadds      fr13,fr1,fr7,fr12   // fr13 = m33 * z + m32 * y + m31 * x (SUMZ)

    // V1
    psq_l       fr6, 0(r8), 0, 3    // fr6 = y, x
    psq_l       fr7, 6(r8), 1, 3    // fr7 = z, 1
    ps_mul      fr8, fr0, fr6       // fr8 = m12 * y, m11 * x
    ps_mul      fr10, fr2, fr6      // fr10 = m22 * y, m21 * x
    ps_mul      fr12, fr4, fr6      // fr12 = m32 * y, m31 * x

    ps_sum0     fr8, fr8, fr8, fr8  // fr8 = m12 * y + m11 * x
    ps_sum0     fr10,fr10,fr10,fr10 // fr10 = m22 * y + m21 * x
    ps_sum0     fr12,fr12,fr12,fr12 // fr12 = m32 * y + m31 * x

    addi        r8, r8, 8
    fmadds      fr19,fr3,fr7,fr8    // fr19 = m13 * z + m12 * y + m11 * x  (SUMX)
    fmadds      fr20,fr5,fr7,fr10   // fr20 = m23 * z + m22 * y + m21 * x (SUMY)
    fmadds      fr21,fr1,fr7,fr12   // fr21 = m33 * z + m32 * y + m31 * x (SUMZ)

    // V2
    psq_l       fr6, 0(r8), 0, 3    // fr6 = y, x
    psq_l       fr7, 6(r8), 1, 3    // fr7 = z, 1
    ps_mul      fr8, fr0, fr6       // fr8 = m12 * y, m11 * x
    ps_mul      fr10, fr2, fr6      // fr10 = m22 * y, m21 * x
    ps_mul      fr12, fr4, fr6      // fr12 = m32 * y, m31 * x

    ps_sum0     fr8, fr8, fr8, fr8  // fr8 = m12 * y + m11 * x
    ps_sum0     fr10,fr10,fr10,fr10 // fr10 = m22 * y + m21 * x
    ps_sum0     fr12,fr12,fr12,fr12 // fr12 = m32 * y + m31 * x

    addi        r8, r8, 8
    fmadds      fr22,fr3,fr7,fr8    // fr22 = m13 * z + m12 * y + m11 * x  (SUMX)
    fmadds      fr23,fr5,fr7,fr10   // fr23 = m23 * z + m22 * y + m21 * x (SUMY)
    fmadds      fr24,fr1,fr7,fr12   // fr24 = m33 * z + m32 * y + m31 * x (SUMZ)

    // load TRX, TRY, TRZ to float
    lfs         fr3, 40(r4)         // gteTRX (r4 + 10 * 4)
    lfs         fr4, 44(r4)         // gteTRY (r4 + 11 * 4)
    lfs         fr5, 48(r4)         // gteTRZ (r4 + 12 * 4)

    ps_merge00  fr9, fr9, fr11              // fr9 = SUMX, SUMY
    ps_merge00  fr3, fr3, fr4               // fr3 = TRX, TRY
    ps_merge00  fr19, fr19, fr20            // fr19 = SUMX, SUMY
    ps_merge00  fr22, fr22, fr23            // fr22 = SUMX, SUMY

    ps_madd     fr2, fr9, fr14ShiftR12, fr3 // fr2 = SUMX >> 12 + TRX, SUMY >> 12 + TRY
    fmadds      fr7, fr13, fr14ShiftR12,fr5 // fr5 = SUMZ >> 12 + TRZ, 0
    ps_merge10  fr9, fr2, fr2               // fr9 = SUMY >> 12 + TRY

    ps_madd     fr10,fr19, fr14ShiftR12,fr3 // fr10 = SUMX >> 12 + TRX, SUMY >> 12 + TRY
    fmadds      fr11,fr21, fr14ShiftR12,fr5 // fr11 = SUMZ >> 12 + TRZ, 0
    ps_merge10  fr12, fr10, fr10            // fr9 = SUMY >> 12 + TRY

    ps_madd     fr27,fr22, fr14ShiftR12,fr3 // fr27 = SUMX >> 12 + TRX, SUMY >> 12 + TRY
    fmadds      fr28,fr24, fr14ShiftR12,fr5 // fr28 = SUMZ >> 12 + TRZ, 0
    ps_merge10  fr29, fr27, fr27            // fr29 = SUMY >> 12 + TRY

    // Save MACX, MACY to tmpGteAddr
    psq_st      fr9, 8(r4), 0, 3     // save MACY, MACX to tmp gteIR2, gteIR1(r4 + 4 * 2)
    psq_st      fr12, 12(r4), 0, 3   // save MACY, MACX to tmp gteIR2, gteIR1(r4 + 4 * 3)
    psq_st      fr29, 16(r4), 0, 3   // save MACY, MACX to tmp gteIR2, gteIR1(r4 + 4 * 4)

    // only v2 save MAC
    fctiw       fr3, fr27            // MACX => int
    fctiw       fr4, fr29            // MACY => int
    fctiw       fr6, fr28            // MACZ => int

    addi        r5, r3, 100
    addi        r6, r3, 104
    addi        r7, r3, 108

    stfiwx      fr3, 0, r5           // store MACX (((s32 *)regs->CP2D.r)[25])
    stfiwx      fr4, 0, r6           // store MACY (((s32 *)regs->CP2D.r)[26])
    stfiwx      fr6, 0, r7           // store MACZ (((s32 *)regs->CP2D.r)[27])

    // only v2 Set gteIR1, gteIR2, gteIR3
    psq_st      fr27, 38(r3), 1, 3   // (regs->CP2D.p[9].sw.l)
    psq_st      fr29, 42(r3), 1, 3   // (regs->CP2D.p[10].sw.l)
    psq_st      fr28, 46(r3), 1, 3   // (regs->CP2D.p[11].sw.l)

    // gteSZX = limD(gteMAC3);
    psq_st      fr7, 70(r3), 1, 4    // gteSZX  (regs->CP2D.p[17, 18, 19].w.l)
    psq_st      fr11, 74(r3), 1, 4   // gteSZX  (regs->CP2D.p[17, 18, 19].w.l)
    psq_st      fr28, 78(r3), 1, 4   // gteSZX  (regs->CP2D.p[17, 18, 19].w.l)

    // quotient = limE(DIVIDE_INT(gteH, gteSZX));
    lhz         r6, 70(r3)           // r6 = gteSZX  (regs->CP2D.p[17, 18, 19].w.l)
    addi        r10, r6, 0
    bl          NATIVE_DIVIDE        // fr5 = quotient
    ps_mr       fr3, fr5

    lhz         r6, 74(r3)           // r6 = gteSZX  (regs->CP2D.p[17, 18, 19].w.l)
    cmpl        3, 0, r10, r6
    beq         3, LOAD_RPTT_CACHE_DIV1
    addi        r10, r6, 0
    bl          NATIVE_DIVIDE        // fr5 = quotient
LOAD_RPTT_CACHE_DIV1:
    ps_mr       fr4, fr5

    lhz         r6, 78(r3)           // r6 = gteSZX  (regs->CP2D.p[17, 18, 19].w.l)
    cmpl        3, 0, r10, r6
    beq         3, LOAD_RPTT_CACHE_DIV2
    bl          NATIVE_DIVIDE        // fr5 = quotient

LOAD_RPTT_CACHE_DIV2:
    psq_l       fr9, 8(r4), 0, 3              // load tmp gteIR2, gteIR1(r4 + 4 * 2)
    psq_l       fr10, 12(r4), 0, 3            // load tmp gteIR2, gteIR1(r4 + 4 * 3)
    psq_l       fr11, 16(r4), 0, 3            // load tmp gteIR2, gteIR1(r4 + 4 * 4)

    // (s64)gteOFX + ((s64)gteIR1 * quotient)
    // (s64)gteOFY + ((s64)gteIR2 * quotient)
    lfs         fr0, 52(r4)                  // gteOFX (r4 + 13 * 4)
    lfs         fr1, 56(r4)                  // gteOFY (r4 + 14 * 4)

    ps_merge00  fr3, fr3, fr3
    ps_merge00  fr0, fr1, fr0                // fr0 = gteOFY, gteOFX
    ps_merge00  fr4, fr4, fr4
    ps_merge00  fr5, fr5, fr5

    ps_madd     fr6, fr9, fr3, fr0           // fr6 = (s64)gteOFY + ((s64)gteIR2 * quotient) / (s64)gteOFX + ((s64)gteIR1 * quotient)
    ps_madd     fr7, fr10, fr4, fr0          // fr7 = (s64)gteOFY + ((s64)gteIR2 * quotient) / (s64)gteOFX + ((s64)gteIR1 * quotient)
    ps_madd     fr8, fr11, fr5, fr0          // fr8 = (s64)gteOFY + ((s64)gteIR2 * quotient) / (s64)gteOFX + ((s64)gteIR1 * quotient)

    // save limG1(gteSXN), limG2(gteSYN)
    psq_st      fr6, 48(r3), 0, 6            // gteSXYN (regs->CP2D.r[12, 13, 14])
    psq_st      fr7, 52(r3), 0, 6            // gteSXYN (regs->CP2D.r[12, 13, 14])
    psq_st      fr8, 56(r3), 0, 6            // gteSXYN (regs->CP2D.r[12, 13, 14])

    // (s64)gteDQB + ((s64)gteDQA * quotient)
    lfs         fr1, 72(r4)                  // gteDQB (r4 + 18 * 4)
    psq_l       fr0, 238(r3), 1, 3           // fr0 = gteDQA (regs->CP2C.p[27].sw.l) / 1.0
    addi        r5, r3, 96
    fmadds      fr0, fr0, fr5, fr1           // fr0 = ((s64)gteDQA * quotient) + (s64)gteDQB

    // gteIR0 = limH(tmp >> 12)
    psq_st      fr0, 34(r3), 1, 1

    // gteMAC0 = F(tmp);
    fctiw       fr1, fr0                     // fr0 => int
    li          r7, 0x1000
    stfiwx      fr1, 0, r5                   // store gteMAC0  (((s32 *)regs->CP2D.r)[24])
    lhz         r6, 34(r3)
    cmpli       3, 0, r6, 0x1000
    blt         3, rtptEnd
    sth         r7, 34(r3)
rtptEnd:
    mtspr       lr, r9
    blr


.globl gtePsMvmva1
// use gteR Matrix
// r3: CP2D
// r4: cvAddr
// r5: vectorAddr
// r6: shift
// r7: lm
// r8: gteTbl15Addr, gteTbl16Addr
gtePsMvmva1:
    lfs         fr14ShiftR12, UnitShiftR12@sdarel(r13)  // fr14 = 1 >> 12, 1 >> 12
    lfs         fr20, UnitShiftL12@sdarel(r13)          // fr20 = 1 << 12, 1 << 12

    cmpli       3, 0, r5, 40
    blt         3, mvmval1UseVecXYZ
    psq_l       fr6, 42(r3), 1, 3    // (regs->CP2D.p[10].sw.l) y
    psq_l       fr8, 38(r3), 1, 3    // (regs->CP2D.p[9].sw.l)  x
    psq_l       fr7, 46(r3), 1, 3    // (regs->CP2D.p[11].sw.l) z
    ps_merge00  fr6, fr6, fr8
    b           mvmval1VecMulMatrix

mvmval1UseVecXYZ:
    addi        r10, r5, 6
    psq_lx      fr6, r5, r3, 0, 3   // fr6 = y, x
    psq_lx      fr7, r10, r3, 1, 3  // fr7 = z, 1

mvmval1VecMulMatrix:
    psq_l       fr0, 128(r3), 0, 3  // fr0 = m12, m11 GQR3 = 0(s16)
    psq_l       fr2, 138(r3), 1, 3  // fr2 = m22
    psq_l       fr9, 132(r3), 1, 3  // fr9 = m21
    ps_mul      fr8, fr0, fr6       // fr8 = m12 * y, m11 * x
    ps_merge00  fr2, fr2, fr9       // fr2 = m22, m21
    psq_l       fr4, 140(r3), 0, 3  // fr4 = m32, m31
    ps_mul      fr10, fr2, fr6      // fr10 = m22 * y, m21 * x
    ps_mul      fr12, fr4, fr6      // fr12 = m32 * y, m31 * x
    psq_l       fr3, 134(r3), 1, 3  // fr3 = m13, 1
    ps_sum0     fr8, fr8, fr8, fr8  // fr8 = m12 * y + m11 * x
    psq_l       fr5, 136(r3), 1, 3  // fr5 = m23, 1
    ps_sum0     fr10,fr10,fr10,fr10 // fr10 = m22 * y + m21 * x
    psq_l       fr1, 146(r3), 1, 3  // fr1 = m33, 1
    ps_sum0     fr12,fr12,fr12,fr12 // fr12 = m32 * y + m31 * x
    fmadds      fr9,fr3,fr7,fr8     // fr9 = m13 * z + m12 * y + m11 * x  (SUMX)
    fmadds      fr11,fr5,fr7,fr10   // fr11 = m23 * z + m22 * y + m21 * x (SUMY)
    fmadds      fr13,fr1,fr7,fr12   // fr13 = m33 * z + m32 * y + m31 * x (SUMZ)

    cmpli       3, 0, r4, 90
    bgt         3, mvmval1UseZeroCv
    // load CVX, CVY, CVZ to float
    addi        r5, r4, 4
    addi        r10, r4, 8
    lfsx        fr3, r4, r8
    lfsx        fr4, r5, r8
    lfsx        fr5, r10, r8
    ps_merge00  fr3, fr3, fr4               // fr3 = CVX, CVY
    b           mvmval1ChkSum

mvmval1UseZeroCv:
    lfs         fr3, UnitZero@sdarel(r13)
    lfs         fr5, UnitZero@sdarel(r13)

mvmval1ChkSum:
    ps_merge00  fr9, fr9, fr11              // fr9 = SUMX, SUMY
    cmpli       3, 0, r6, 1
    blt         3, mvmval1NoShift
    ps_madd     fr2, fr9, fr14ShiftR12, fr3 // fr2 = SUMX >> 12 + CVX, SUMY >> 12 + CVY
    fmadds      fr5, fr13, fr14ShiftR12,fr5 // fr5 = SUMZ >> 12 + CVZ, 0
    ps_merge10  fr9, fr2, fr2               // fr9 = SUMY >> 12 + CVY
    b           mvmval1SaveSum

mvmval1NoShift:
    ps_madd     fr2, fr3, fr20, fr9         // fr2 = CVX << 12 + SUMX, CVY << 12 + SUMY
    fmadds      fr5, fr5, fr20,fr13         // fr5 = CVZ << 12 + SUMZ, 0
    ps_merge10  fr9, fr2, fr2               // fr9 = CVY << 12 + SUMY

mvmval1SaveSum:
    // save MAC
    fctiw       fr3, fr2                    // MACX => int
    fctiw       fr4, fr9                    // MACY => int
    fctiw       fr6, fr5                    // MACZ => int

    addi        r4, r3, 100
    addi        r5, r3, 104
    addi        r6, r3, 108

    stfiwx      fr3, 0, r4                  // store MACX (((s32 *)regs->CP2D.r)[25])
    stfiwx      fr4, 0, r5                  // store MACY (((s32 *)regs->CP2D.r)[26])
    stfiwx      fr6, 0, r6                  // store MACZ (((s32 *)regs->CP2D.r)[27])

    // Set gteIR1, gteIR2, gteIR3
    psq_st      fr2, 38(r3), 1, 3           // (regs->CP2D.p[9].sw.l)
    psq_st      fr9, 42(r3), 1, 3           // (regs->CP2D.p[10].sw.l)
    psq_st      fr5, 46(r3), 1, 3           // (regs->CP2D.p[11].sw.l)

    cmpli       3, 0, r7, 1
    blt         3, mvmval1End
    lha         r4, 38(r3)
    lha         r5, 42(r3)
    lha         r6, 46(r3)
    li          r7, 0
    cmpi        3, 0, r4, 0
    bgt         3, mvmval1ChkIR2
    sth         r7, 38(r3)
mvmval1ChkIR2:
    cmpi        3, 0, r5, 0
    bgt         3, mvmval1ChkIR3
    sth         r7, 42(r3)
mvmval1ChkIR3:
    cmpi        3, 0, r6, 0
    bgt         3, mvmval1End
    sth         r7, 46(r3)

mvmval1End:
    blr


.globl gtePsMvmva2
// use gteL Matrix
// r3: CP2D
// r4: cvAddr
// r5: vectorAddr
// r6: shift
// r7: lm
// r8: gteTbl15Addr, gteTbl16Addr
gtePsMvmva2:
    lfs         fr14ShiftR12, UnitShiftR12@sdarel(r13)  // fr14 = 1 >> 12, 1 >> 12
    lfs         fr20, UnitShiftL12@sdarel(r13)          // fr20 = 1 << 12, 1 << 12

    cmpli       3, 0, r5, 40
    blt         3, mvmval2UseVecXYZ
    psq_l       fr6, 42(r3), 1, 3    // (regs->CP2D.p[10].sw.l) y
    psq_l       fr8, 38(r3), 1, 3    // (regs->CP2D.p[9].sw.l)  x
    psq_l       fr7, 46(r3), 1, 3    // (regs->CP2D.p[11].sw.l) z
    ps_merge00  fr6, fr6, fr8
    b           mvmval2VecMulMatrix

mvmval2UseVecXYZ:
    addi        r10, r5, 6
    psq_lx      fr6, r5, r3, 0, 3   // fr6 = y, x
    psq_lx      fr7, r10, r3, 1, 3  // fr7 = z, 1

mvmval2VecMulMatrix:
    psq_l       fr0, 160(r3), 0, 3  // fr0 = m12, m11 GQR3 = 0(s16)
    psq_l       fr2, 170(r3), 1, 3  // fr2 = m22
    psq_l       fr9, 164(r3), 1, 3  // fr9 = m21
    ps_mul      fr8, fr0, fr6       // fr8 = m12 * y, m11 * x
    ps_merge00  fr2, fr2, fr9       // fr2 = m22, m21
    psq_l       fr4, 172(r3), 0, 3  // fr4 = m32, m31
    ps_mul      fr10, fr2, fr6      // fr10 = m22 * y, m21 * x
    ps_mul      fr12, fr4, fr6      // fr12 = m32 * y, m31 * x
    psq_l       fr3, 166(r3), 1, 3  // fr3 = m13, 1
    ps_sum0     fr8, fr8, fr8, fr8  // fr8 = m12 * y + m11 * x
    psq_l       fr5, 168(r3), 1, 3  // fr5 = m23, 1
    ps_sum0     fr10,fr10,fr10,fr10 // fr10 = m22 * y + m21 * x
    psq_l       fr1, 178(r3), 1, 3  // fr1 = m33, 1
    ps_sum0     fr12,fr12,fr12,fr12 // fr12 = m32 * y + m31 * x
    fmadds      fr9,fr3,fr7,fr8     // fr9 = m13 * z + m12 * y + m11 * x  (SUMX)
    fmadds      fr11,fr5,fr7,fr10   // fr11 = m23 * z + m22 * y + m21 * x (SUMY)
    fmadds      fr13,fr1,fr7,fr12   // fr13 = m33 * z + m32 * y + m31 * x (SUMZ)

    cmpli       3, 0, r4, 90
    bgt         3, mvmval2UseZeroCv
    // load CVX, CVY, CVZ to float
    addi        r5, r4, 4
    addi        r10, r4, 8
    lfsx        fr3, r4, r8
    lfsx        fr4, r5, r8
    lfsx        fr5, r10, r8
    ps_merge00  fr3, fr3, fr4               // fr3 = CVX, CVY
    b           mvmval2ChkSum

mvmval2UseZeroCv:
    lfs         fr3, UnitZero@sdarel(r13)
    lfs         fr5, UnitZero@sdarel(r13)

mvmval2ChkSum:
    ps_merge00  fr9, fr9, fr11              // fr9 = SUMX, SUMY
    cmpli       3, 0, r6, 1
    blt         3, mvmval2NoShift
    ps_madd     fr2, fr9, fr14ShiftR12, fr3 // fr2 = SUMX >> 12 + CVX, SUMY >> 12 + CVY
    fmadds      fr5, fr13, fr14ShiftR12,fr5 // fr5 = SUMZ >> 12 + CVZ, 0
    ps_merge10  fr9, fr2, fr2               // fr9 = SUMY >> 12 + CVY
    b           mvmval2SaveSum

mvmval2NoShift:
    ps_madd     fr2, fr3, fr20, fr9         // fr2 = CVX << 12 + SUMX, CVY << 12 + SUMY
    fmadds      fr5, fr5, fr20,fr13         // fr5 = CVZ << 12 + SUMZ, 0
    ps_merge10  fr9, fr2, fr2               // fr9 = CVY << 12 + SUMY

mvmval2SaveSum:
    // save MAC
    fctiw       fr3, fr2                    // MACX => int
    fctiw       fr4, fr9                    // MACY => int
    fctiw       fr6, fr5                    // MACZ => int

    addi        r4, r3, 100
    addi        r5, r3, 104
    addi        r6, r3, 108

    stfiwx      fr3, 0, r4                  // store MACX (((s32 *)regs->CP2D.r)[25])
    stfiwx      fr4, 0, r5                  // store MACY (((s32 *)regs->CP2D.r)[26])
    stfiwx      fr6, 0, r6                  // store MACZ (((s32 *)regs->CP2D.r)[27])

    // Set gteIR1, gteIR2, gteIR3
    psq_st      fr2, 38(r3), 1, 3           // (regs->CP2D.p[9].sw.l)
    psq_st      fr9, 42(r3), 1, 3           // (regs->CP2D.p[10].sw.l)
    psq_st      fr5, 46(r3), 1, 3           // (regs->CP2D.p[11].sw.l)

    cmpli       3, 0, r7, 1
    blt         3, mvmval2End
    lha         r4, 38(r3)
    lha         r5, 42(r3)
    lha         r6, 46(r3)
    li          r7, 0
    cmpi        3, 0, r4, 0
    bgt         3, mvmval2ChkIR2
    sth         r7, 38(r3)
mvmval2ChkIR2:
    cmpi        3, 0, r5, 0
    bgt         3, mvmval2ChkIR3
    sth         r7, 42(r3)
mvmval2ChkIR3:
    cmpi        3, 0, r6, 0
    bgt         3, mvmval2End
    sth         r7, 46(r3)

mvmval2End:
    blr


.globl gtePsMvmva3
// use gteC Matrix
// r3: CP2D
// r4: cvAddr
// r5: vectorAddr
// r6: shift
// r7: lm
// r8: gteTbl15Addr, gteTbl16Addr
gtePsMvmva3:
    lfs         fr14ShiftR12, UnitShiftR12@sdarel(r13)  // fr14 = 1 >> 12, 1 >> 12
    lfs         fr20, UnitShiftL12@sdarel(r13)          // fr20 = 1 << 12, 1 << 12

    cmpli       3, 0, r5, 40
    blt         3, mvmval3UseVecXYZ
    psq_l       fr6, 42(r3), 1, 3    // (regs->CP2D.p[10].sw.l) y
    psq_l       fr8, 38(r3), 1, 3    // (regs->CP2D.p[9].sw.l)  x
    psq_l       fr7, 46(r3), 1, 3    // (regs->CP2D.p[11].sw.l) z
    ps_merge00  fr6, fr6, fr8
    b           mvmval3VecMulMatrix

mvmval3UseVecXYZ:
    addi        r10, r5, 6
    psq_lx      fr6, r5, r3, 0, 3   // fr6 = y, x
    psq_lx      fr7, r10, r3, 1, 3  // fr7 = z, 1

mvmval3VecMulMatrix:
    psq_l       fr0, 192(r3), 0, 3  // fr0 = m12, m11 GQR3 = 0(s16)
    psq_l       fr2, 202(r3), 1, 3  // fr2 = m22
    psq_l       fr9, 196(r3), 1, 3  // fr9 = m21
    ps_mul      fr8, fr0, fr6       // fr8 = m12 * y, m11 * x
    ps_merge00  fr2, fr2, fr9       // fr2 = m22, m21
    psq_l       fr4, 204(r3), 0, 3  // fr4 = m32, m31
    ps_mul      fr10, fr2, fr6      // fr10 = m22 * y, m21 * x
    ps_mul      fr12, fr4, fr6      // fr12 = m32 * y, m31 * x
    psq_l       fr3, 198(r3), 1, 3  // fr3 = m13, 1
    ps_sum0     fr8, fr8, fr8, fr8  // fr8 = m12 * y + m11 * x
    psq_l       fr5, 200(r3), 1, 3  // fr5 = m23, 1
    ps_sum0     fr10,fr10,fr10,fr10 // fr10 = m22 * y + m21 * x
    psq_l       fr1, 210(r3), 1, 3  // fr1 = m33, 1
    ps_sum0     fr12,fr12,fr12,fr12 // fr12 = m32 * y + m31 * x
    fmadds      fr9,fr3,fr7,fr8     // fr9 = m13 * z + m12 * y + m11 * x  (SUMX)
    fmadds      fr11,fr5,fr7,fr10   // fr11 = m23 * z + m22 * y + m21 * x (SUMY)
    fmadds      fr13,fr1,fr7,fr12   // fr13 = m33 * z + m32 * y + m31 * x (SUMZ)

    cmpli       3, 0, r4, 90
    bgt         3, mvmval3UseZeroCv
    // load CVX, CVY, CVZ to float
    addi        r5, r4, 4
    addi        r10, r4, 8
    lfsx        fr3, r4, r8
    lfsx        fr4, r5, r8
    lfsx        fr5, r10, r8
    ps_merge00  fr3, fr3, fr4               // fr3 = CVX, CVY
    b           mvmval3ChkSum

mvmval3UseZeroCv:
    lfs         fr3, UnitZero@sdarel(r13)
    lfs         fr5, UnitZero@sdarel(r13)

mvmval3ChkSum:
    ps_merge00  fr9, fr9, fr11              // fr9 = SUMX, SUMY
    cmpli       3, 0, r6, 1
    blt         3, mvmval3NoShift
    ps_madd     fr2, fr9, fr14ShiftR12, fr3 // fr2 = SUMX >> 12 + CVX, SUMY >> 12 + CVY
    fmadds      fr5, fr13, fr14ShiftR12,fr5 // fr5 = SUMZ >> 12 + CVZ, 0
    ps_merge10  fr9, fr2, fr2               // fr9 = SUMY >> 12 + CVY
    b           mvmval3SaveSum

mvmval3NoShift:
    ps_madd     fr2, fr3, fr20, fr9         // fr2 = CVX << 12 + SUMX, CVY << 12 + SUMY
    fmadds      fr5, fr5, fr20,fr13         // fr5 = CVZ << 12 + SUMZ, 0
    ps_merge10  fr9, fr2, fr2               // fr9 = CVY << 12 + SUMY

mvmval3SaveSum:
    // save MAC
    fctiw       fr3, fr2                    // MACX => int
    fctiw       fr4, fr9                    // MACY => int
    fctiw       fr6, fr5                    // MACZ => int

    addi        r4, r3, 100
    addi        r5, r3, 104
    addi        r6, r3, 108

    stfiwx      fr3, 0, r4                  // store MACX (((s32 *)regs->CP2D.r)[25])
    stfiwx      fr4, 0, r5                  // store MACY (((s32 *)regs->CP2D.r)[26])
    stfiwx      fr6, 0, r6                  // store MACZ (((s32 *)regs->CP2D.r)[27])

    // Set gteIR1, gteIR2, gteIR3
    psq_st      fr2, 38(r3), 1, 3           // (regs->CP2D.p[9].sw.l)
    psq_st      fr9, 42(r3), 1, 3           // (regs->CP2D.p[10].sw.l)
    psq_st      fr5, 46(r3), 1, 3           // (regs->CP2D.p[11].sw.l)

    cmpli       3, 0, r7, 1
    blt         3, mvmval3End
    lha         r4, 38(r3)
    lha         r5, 42(r3)
    lha         r6, 46(r3)
    li          r7, 0
    cmpi        3, 0, r4, 0
    bgt         3, mvmval3ChkIR2
    sth         r7, 38(r3)
mvmval3ChkIR2:
    cmpi        3, 0, r5, 0
    bgt         3, mvmval3ChkIR3
    sth         r7, 42(r3)
mvmval3ChkIR3:
    cmpi        3, 0, r6, 0
    bgt         3, mvmval3End
    sth         r7, 46(r3)

mvmval3End:
    blr


.globl gtePsNclip
// r3: CP2D(vy0, vx0, 0, vz0, vy1, vx1, 0, vz1, vy2, vx2, 0, vz2)
gtePsNclip:
    // load gteSY0, gteSY1, gteSY2
    lha       r4, 48(r3)      // gteSY0 (regs->CP2D.r[12])
    lha       r5, 52(r3)      // gteSY1 (regs->CP2D.r[13])
    lha       r6, 56(r3)      // gteSY2 (regs->CP2D.r[14])

    // gteSY0 - gteSY1, gteSY1 - gteSY2, gteSY2 - gteSY0
    sub       r7, r4, r5
    sub       r8, r5, r6
    sub       r9, r6, r4

    // load gteSX0, gteSX1, gteSX2
    lha       r4, 50(r3)      // gteSX0 (regs->CP2D.r[12])
    lha       r5, 54(r3)      // gteSX1 (regs->CP2D.r[13])
    lha       r6, 58(r3)      // gteSX2 (regs->CP2D.r[14])

    // gteSX2 * (gteSY0 - gteSY1) + gteSX0 * (gteSY1 - gteSY2) + gteSX1 * (gteSY2 - gteSY0)
    mullw     r4, r4, r8
    mullw     r5, r5, r9
    mullw     r6, r6, r7

    add       r8, r4, r5
    add       r8, r8, r6

    stw       r8, 96(r3)       // gteMAC0  (((s32 *)regs->CP2D.r)[24])

    blr

.globl gtePsAvsz3
// r3: CP2D(vy0, vx0, 0, vz0, vy1, vx1, 0, vz1, vy2, vx2, 0, vz2)
gtePsAvsz3:
    // load gteSZ1, gteSZ2, gteSZ3, gteZSF3
    psq_l       fr0, 70(r3), 0, 4  // gteSZ1  (regs->CP2D.p[17].w.l)
    psq_l       fr1, 74(r3), 0, 4  // gteSZ2  (regs->CP2D.p[18].w.l)
    psq_l       fr2, 78(r3), 0, 4  // gteSZ3  (regs->CP2D.p[19].w.l)
    psq_l       fr3, 246(r3), 0, 3 // gteZSF3 (regs->CP2C.p[29].sw.l) 128 + 118

    // gteZSF3 * (gteSZ1 + gteSZ2 + gteSZ3)
    fadds       fr4, fr0, fr1
    fadds       fr4, fr4, fr2
    fmuls       fr3, fr3, fr4

    // set gteMAC0, gteOTZ = limD(gteMAC0 >> 12)
    //fctiw       fr0, fr3
    //addi        r4, r3, 96         // gteMAC0  (((s32 *)regs->CP2D.r)[24])
    psq_st      fr3, 30(r3), 1, 1  // gteOTZ  (regs->CP2D.p[7].w.l)
    //stfiwx      fr0, 0, r4

    blr

.globl gtePsAvsz4
// r3: CP2D(vy0, vx0, 0, vz0, vy1, vx1, 0, vz1, vy2, vx2, 0, vz2)
gtePsAvsz4:
    // load gteSZ0, gteSZ1, gteSZ2, gteSZ3, gteZSF4
    psq_l       fr0, 66(r3), 0, 4  // gteSZ0  (regs->CP2D.p[16].w.l)
    psq_l       fr1, 70(r3), 0, 4  // gteSZ1  (regs->CP2D.p[17].w.l)
    psq_l       fr2, 74(r3), 0, 4  // gteSZ2  (regs->CP2D.p[18].w.l)
    psq_l       fr3, 78(r3), 0, 4  // gteSZ3  (regs->CP2D.p[19].w.l)
    psq_l       fr4, 250(r3), 0, 3 // gteZSF4 (regs->CP2C.p[30].sw.l) 128 + 122

    // gteZSF4 * (gteSZ0 + gteSZ1 + gteSZ2 + gteSZ3)
    fadds       fr5, fr0, fr1
    fadds       fr6, fr2, fr3
    fadds       fr5, fr5, fr6
    fmuls       fr3, fr4, fr5

    // set gteMAC0, gteOTZ = limD(gteMAC0 >> 12)
    //fctiw       fr0, fr3
    //addi        r4, r3, 96         // gteMAC0  (((s32 *)regs->CP2D.r)[24])
    psq_st      fr3, 30(r3), 1, 1  // gteOTZ  (regs->CP2D.p[7].w.l)
    //stfiwx      fr0, 0, r4

    blr


.globl gtePsNccs
// r3: CP2D(vy0, vx0, 0, vz0, vy1, vx1, 0, vz1, vy2, vx2, 0, vz2)
//     CP2C(m12, m11, m21, m13, m23, m22, m32, m31, 0, m33)(r3 + 128)
//     CP2C(L12, L11, L21, L13, L23, L22, L32, L31, 0, L33)(r3 + 128 + 8 * 4)
//     CP2C(LR2, LR1, LG1, LR3, LG3, LG2, LB2, LB1, 0, LB3)(r3 + 128 + 16 * 4)
// r4: gteTbl15Addr, gteTbl16Addr, gteTmpAddr
gtePsNccs:
    lfs         fr14ShiftR12, UnitShiftR12@sdarel(r13)  // fr14 = 1 >> 12, 1 >> 12
    lfs         fr17ShiftR8, UnitShiftR8@sdarel(r13)    // fr17 = 1 >> 8, 1 >> 8

    psq_l       fr0, 160(r3), 0, 3  // fr0 = L12, L11 GQR3 = 0(s16)
    psq_l       fr6, 0(r3), 0, 3    // fr6 = y, x

    psq_l       fr2, 170(r3), 1, 3  // fr2 = L22
    psq_l       fr9, 164(r3), 1, 3  // fr9 = L21

    ps_mul      fr8, fr0, fr6       // fr8 = L12 * y, L11 * x
    ps_merge00  fr2, fr2, fr9       // fr2 = L22, L21
    psq_l       fr4, 172(r3), 0, 3  // fr4 = L32, L31
    ps_mul      fr10, fr2, fr6      // fr10 = L22 * y, L21* x
    psq_l       fr7, 6(r3), 1, 3    // fr7 = z, 1
    ps_mul      fr12, fr4, fr6      // fr12 = L32 * y, L31 * x

    psq_l       fr3, 166(r3), 1, 3  // fr3 = L13, 1
    ps_sum0     fr8, fr8, fr8, fr8  // fr8 = L12 * y + L11 * x
    psq_l       fr5, 168(r3), 1, 3  // fr5 = L23, 1
    ps_sum0     fr10,fr10,fr10,fr10 // fr10 = L22 * y + L21 * x
    psq_l       fr1, 178(r3), 1, 3  // fr1 = L33, 1
    ps_sum0     fr12,fr12,fr12,fr12 // fr12 = L32 * y + L31 * x
    fmadds      fr9,fr3,fr7,fr8     // fr9 = L13 * z + L12 * y + L11 * x  (SUMX)
    fmadds      fr11,fr5,fr7,fr10   // fr11 = L23 * z + L22 * y + L21 * x (SUMY)
    fmadds      fr13,fr1,fr7,fr12   // fr13 = L33 * z + L32 * y + L31 * x (SUMZ)

    ps_merge00  fr9, fr11, fr9              // fr9 = SUMY, SUMX
    fmuls       fr5, fr13, fr14ShiftR12     // fr5 = SUMZ >> 12, 0
    ps_mul      fr2, fr9, fr14ShiftR12      // fr2 = SUMY >> 12, SUMX >> 12

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
    fmadds      fr9,fr3,fr7,fr8     // fr9 = LR3 * z + LR2 * y + LR1 * x  (SUMX)
    fmadds      fr11,fr5,fr7,fr10   // fr11 = LG3 * z + LG2 * y + LG1 * x (SUMY)
    fmadds      fr13,fr1,fr7,fr12   // fr13 = LB3 * z + LB2 * y + LB1 * x (SUMZ)

    ps_merge00  fr9, fr11, fr9      // fr9 = SUMY, SUMX

    // load gteRBK, gteGBK, gteBBK to float
    lfs         fr3, 60(r4)         // gteRBK (r4 + 15 * 4)
    lfs         fr4, 64(r4)         // gteGBK (r4 + 16 * 4)
    lfs         fr5, 68(r4)         // gteBBK (r4 + 17 * 4)

    ps_merge00  fr3, fr4, fr3               // fr3 = GBK, RBK
    fmadds      fr5, fr13, fr14ShiftR12,fr5 // fr5 = SUMZ >> 12 + BBK, 0
    ps_madd     fr2, fr9, fr14ShiftR12, fr3 // fr2 = SUMY >> 12 + GBK, SUMX >> 12 + RBK

    // Save MACX, MACY, MACZ to tmpGteAddr
    psq_st      fr5, 12(r4), 1, 7    // save MACZ to tmp gteIR3(r4 + 4 * 3)
    psq_st      fr2, 8(r4), 0, 7     // save MACY, MACX to tmp gteIR2, gteIR1(r4 + 4 * 2)

    // RGB * IR (gteG (regs->CP2D.p[6].b.h))
    psq_l       fr0, 26(r3), 0, 2    // fr0 = gteG, gteR GQR2 = 0(u8)
    psq_l       fr3, 25(r3), 1, 2    // fr3 = gteB, 1
    psq_l       fr5, 12(r4), 1, 7    // load tmp gteIR3(r4 + 4 * 3)
    psq_l       fr9, 8(r4), 0, 7     // load tmp gteIR2, gteIR1(r4 + 4 * 2)
    fmuls       fr5, fr3, fr5        // fr5 = gteB * gteIR3
    ps_mul      fr9, fr0, fr9        // fr5 = gteG * gteIR2, gteR * gteIR1

    // gteIR1 = gteMAC1 = ((s32)gteR * gteIR1) >> 8
    // gteIR2 = gteMAC2 = ((s32)gteG * gteIR2) >> 8
    // gteIR3 = gteMAC3 = ((s32)gteB * gteIR3) >> 8
    fmuls       fr3, fr5, fr17ShiftR8 // fr3 = gteMAC3 >> 8
    ps_mul      fr4, fr9, fr17ShiftR8 // fr4 = gteMAC2 >> 8, gteMAC1 >> 8

    psq_st      fr3, 46(r3), 1, 3    // gteIR3 (regs->CP2D.p[11].sw.l)
    ps_merge10  fr6, fr4, fr4        // fr6 = gteMAC1 >> 8, gteMAC2 >> 8
    psq_st      fr4, 42(r3), 1, 3    // gteIR2 (regs->CP2D.p[10].sw.l)
    psq_st      fr6, 38(r3), 1, 3    // gteIR1 (regs->CP2D.p[9].sw.l)

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
    psq_st      fr5, 89(r3), 1, 5      // gteB2    (regs->CP2D.p[22].b.h2)
    psq_st      fr9, 90(r3), 0, 5      // gteG2    (regs->CP2D.p[22].b.h), gteR2    (regs->CP2D.p[22].b.l)

    blr


.globl gtePsNcct
// r3: CP2D(vy0, vx0, 0, vz0, vy1, vx1, 0, vz1, vy2, vx2, 0, vz2)
//     CP2C(m12, m11, m21, m13, m23, m22, m32, m31, 0, m33)(r3 + 128)
//     CP2C(L12, L11, L21, L13, L23, L22, L32, L31, 0, L33)(r3 + 128 + 8 * 4)
//     CP2C(LR2, LR1, LG1, LR3, LG3, LG2, LB2, LB1, 0, LB3)(r3 + 128 + 16 * 4)
// r4: gteTbl15Addr, gteTbl16Addr, gteTmpAddr
gtePsNcct:
    addi        r8, r3, 0
    lfs         fr14ShiftR12, UnitShiftR12@sdarel(r13)  // fr14 = 1 >> 12, 1 >> 12
    lfs         fr17ShiftR8, UnitShiftR8@sdarel(r13)    // fr17 = 1 >> 8, 1 >> 8

    // V0
    psq_l       fr0, 160(r3), 0, 3  // fr0 = L12, L11 GQR3 = 0(s16)
    psq_l       fr6, 0(r8), 0, 3    // fr6 = y, x

    psq_l       fr2, 170(r3), 1, 3  // fr2 = L22
    psq_l       fr9, 164(r3), 1, 3  // fr9 = L21

    ps_mul      fr8, fr0, fr6       // fr8 = L12 * y, L11 * x
    ps_merge00  fr2, fr2, fr9       // fr2 = L22, L21
    psq_l       fr4, 172(r3), 0, 3  // fr4 = L32, L31
    ps_mul      fr10, fr2, fr6      // fr10 = L22 * y, L21* x
    psq_l       fr7, 6(r8), 1, 3    // fr7 = z, 1
    ps_mul      fr12, fr4, fr6      // fr12 = L32 * y, L31 * x

    psq_l       fr3, 166(r3), 1, 3  // fr3 = L13, 1
    ps_sum0     fr8, fr8, fr8, fr8  // fr8 = L12 * y + L11 * x
    psq_l       fr5, 168(r3), 1, 3  // fr5 = L23, 1
    ps_sum0     fr10,fr10,fr10,fr10 // fr10 = L22 * y + L21 * x
    psq_l       fr1, 178(r3), 1, 3  // fr1 = L33, 1
    ps_sum0     fr12,fr12,fr12,fr12 // fr12 = L32 * y + L31 * x

    addi        r8, r8, 8
    fmadds      fr9,fr3,fr7,fr8     // fr9 = L13 * z + L12 * y + L11 * x  (SUMX)
    fmadds      fr11,fr5,fr7,fr10   // fr11 = L23 * z + L22 * y + L21 * x (SUMY)
    fmadds      fr13,fr1,fr7,fr12   // fr13 = L33 * z + L32 * y + L31 * x (SUMZ)

    // V1
    psq_l       fr6, 0(r8), 0, 3    // fr6 = y, x
    psq_l       fr7, 6(r8), 1, 3    // fr7 = z, 1
    ps_mul      fr8, fr0, fr6       // fr8 = L12 * y, L11 * x
    ps_mul      fr10, fr2, fr6      // fr10 = L22 * y, L21* x
    ps_mul      fr12, fr4, fr6      // fr12 = L32 * y, L31 * x

    ps_sum0     fr8, fr8, fr8, fr8  // fr8 = L12 * y + L11 * x
    ps_sum0     fr10,fr10,fr10,fr10 // fr10 = L22 * y + L21 * x
    ps_sum0     fr12,fr12,fr12,fr12 // fr12 = L32 * y + L31 * x

    addi        r8, r8, 8
    fmadds      fr19,fr3,fr7,fr8    // fr19 = L13 * z + L12 * y + L11 * x  (SUMX)
    fmadds      fr20,fr5,fr7,fr10   // fr20 = L23 * z + L22 * y + L21 * x (SUMY)
    fmadds      fr21,fr1,fr7,fr12   // fr21 = L33 * z + L32 * y + L31 * x (SUMZ)

    // V2
    psq_l       fr6, 0(r8), 0, 3    // fr6 = y, x
    psq_l       fr7, 6(r8), 1, 3    // fr7 = z, 1
    ps_mul      fr8, fr0, fr6       // fr8 = L12 * y, L11 * x
    ps_mul      fr10, fr2, fr6      // fr10 = L22 * y, L21* x
    ps_mul      fr12, fr4, fr6      // fr12 = L32 * y, L31 * x

    ps_sum0     fr8, fr8, fr8, fr8  // fr8 = L12 * y + L11 * x
    ps_sum0     fr10,fr10,fr10,fr10 // fr10 = L22 * y + L21 * x
    ps_sum0     fr12,fr12,fr12,fr12 // fr12 = L32 * y + L31 * x

    fmadds      fr22,fr3,fr7,fr8    // fr22 = L13 * z + L12 * y + L11 * x  (SUMX)
    fmadds      fr23,fr5,fr7,fr10   // fr23 = L23 * z + L22 * y + L21 * x (SUMY)
    fmadds      fr24,fr1,fr7,fr12   // fr24 = L33 * z + L32 * y + L31 * x (SUMZ)

    ps_merge00  fr9, fr11, fr9              // fr9 = SUMY, SUMX
    ps_merge00  fr19, fr20, fr19            // fr19 = SUMY, SUMX
    ps_merge00  fr22, fr23, fr22            // fr22 = SUMY, SUMX

    fmuls       fr13, fr13, fr14ShiftR12    // fr13 = SUMZ >> 12, 0
    fmuls       fr21, fr21, fr14ShiftR12    // fr21 = SUMZ >> 12, 0
    fmuls       fr24, fr24, fr14ShiftR12    // fr24 = SUMZ >> 12, 0
    ps_mul      fr9, fr9, fr14ShiftR12      // fr9 = SUMY >> 12, SUMX >> 12
    ps_mul      fr19, fr19, fr14ShiftR12    // fr19 = SUMY >> 12, SUMX >> 12
    ps_mul      fr22, fr22, fr14ShiftR12    // fr22 = SUMY >> 12, SUMX >> 12

    // Save MACX, MACY, MACZ to tmpGteAddr
    psq_st      fr13, 8(r4), 1, 7   // save MACZ to tmp gteIR3(r4 + 4 * 2)
    psq_st      fr9, 12(r4), 0, 7   // save MACY, MACX to tmp gteIR2, gteIR1(r4 + 4 * 3)
    psq_st      fr21, 16(r4), 1, 7  // save MACZ to tmp gteIR3(r4 + 4 * 4)
    psq_st      fr19, 20(r4), 0, 7  // save MACY, MACX to tmp gteIR2, gteIR1(r4 + 4 * 5)
    psq_st      fr24, 24(r4), 1, 7  // save MACZ to tmp gteIR3(r4 + 4 * 6)
    psq_st      fr22, 28(r4), 0, 7  // save MACY, MACX to tmp gteIR2, gteIR1(r4 + 4 * 7)

    // V0(LR * IR)
    psq_l       fr0, 192(r3), 0, 3  // fr0 = LR2, LR1 GQR3 = 0(s16)
    psq_l       fr6, 12(r4), 0, 7   // fr6 = y, x

    psq_l       fr2, 202(r3), 1, 3  // fr2 = LG2
    psq_l       fr9, 196(r3), 1, 3  // fr9 = LG1

    ps_mul      fr8, fr0, fr6       // fr8 = LR2 * y, LR1 * x
    ps_merge00  fr2, fr2, fr9       // fr2 = LG2, LG1
    psq_l       fr4, 204(r3), 0, 3  // fr4 = LB2, LB1
    ps_mul      fr10, fr2, fr6      // fr10 = LG2 * y, LG2* x
    psq_l       fr7, 8(r4), 1, 7    // fr7 = z, 1
    ps_mul      fr12, fr4, fr6      // fr12 = LB2 * y, LB1 * x

    psq_l       fr3, 198(r3), 1, 3  // fr3 = LR3, 1
    ps_sum0     fr8, fr8, fr8, fr8  // fr8 = LR2 * y + LR1 * x
    psq_l       fr5, 200(r3), 1, 3  // fr5 = LG3, 1
    ps_sum0     fr10,fr10,fr10,fr10 // fr10 = LG2 * y + LG1 * x
    psq_l       fr1, 210(r3), 1, 3  // fr1 = LB3, 1
    ps_sum0     fr12,fr12,fr12,fr12 // fr12 = LB2 * y + LB1 * x
    fmadds      fr9,fr3,fr7,fr8     // fr9 = LR3 * z + LR2 * y + LR1 * x  (SUMX)
    fmadds      fr11,fr5,fr7,fr10   // fr11 = LG3 * z + LG2 * y + LG1 * x (SUMY)
    fmadds      fr13,fr1,fr7,fr12   // fr13 = LB3 * z + LB2 * y + LB1 * x (SUMZ)

    // V1(LR * IR)
    psq_l       fr6, 20(r4), 0, 7   // fr6 = y, x
    psq_l       fr7, 16(r4), 1, 7   // fr7 = z, 1

    ps_mul      fr8, fr0, fr6       // fr8 = LR2 * y, LR1 * x
    ps_mul      fr10, fr2, fr6      // fr10 = LG2 * y, LG2* x
    ps_mul      fr12, fr4, fr6      // fr12 = LB2 * y, LB1 * x

    ps_sum0     fr8, fr8, fr8, fr8  // fr8 = LR2 * y + LR1 * x
    ps_sum0     fr10,fr10,fr10,fr10 // fr10 = LG2 * y + LG1 * x
    ps_sum0     fr12,fr12,fr12,fr12 // fr12 = LB2 * y + LB1 * x

    fmadds      fr19,fr3,fr7,fr8    // fr19 = LR3 * z + LR2 * y + LR1 * x  (SUMX)
    fmadds      fr20,fr5,fr7,fr10   // fr20 = LG3 * z + LG2 * y + LG1 * x (SUMY)
    fmadds      fr21,fr1,fr7,fr12   // fr21 = LB3 * z + LB2 * y + LB1 * x (SUMZ)

    // V2(LR * IR)
    psq_l       fr6, 28(r4), 0, 7   // fr6 = y, x
    psq_l       fr7, 24(r4), 1, 7   // fr7 = z, 1

    ps_mul      fr8, fr0, fr6       // fr8 = LR2 * y, LR1 * x
    ps_mul      fr10, fr2, fr6      // fr10 = LG2 * y, LG2* x
    ps_mul      fr12, fr4, fr6      // fr12 = LB2 * y, LB1 * x

    ps_sum0     fr8, fr8, fr8, fr8  // fr8 = LR2 * y + LR1 * x
    ps_sum0     fr10,fr10,fr10,fr10 // fr10 = LG2 * y + LG1 * x
    ps_sum0     fr12,fr12,fr12,fr12 // fr12 = LB2 * y + LB1 * x

    fmadds      fr22,fr3,fr7,fr8    // fr22 = LR3 * z + LR2 * y + LR1 * x  (SUMX)
    fmadds      fr23,fr5,fr7,fr10   // fr23 = LG3 * z + LG2 * y + LG1 * x (SUMY)
    fmadds      fr24,fr1,fr7,fr12   // fr24 = LB3 * z + LB2 * y + LB1 * x (SUMZ)

    // load gteRBK, gteGBK, gteBBK to float
    lfs         fr3, 60(r4)         // gteRBK (r4 + 15 * 4)
    lfs         fr4, 64(r4)         // gteGBK (r4 + 16 * 4)
    lfs         fr5, 68(r4)         // gteBBK (r4 + 17 * 4)

    ps_merge00  fr9, fr11, fr9      // fr9 = SUMY, SUMX
    ps_merge00  fr3, fr4, fr3       // fr3 = GBK, RBK
    ps_merge00  fr19, fr20, fr19    // fr19 = SUMY, SUMX
    ps_merge00  fr22, fr23, fr22    // fr9 = SUMY, SUMX

    fmadds      fr13,fr13, fr14ShiftR12,fr5 // fr13 = SUMZ >> 12 + BBK, 0
    ps_madd     fr9, fr9, fr14ShiftR12, fr3 // fr9 = SUMY >> 12 + GBK, SUMX >> 12 + RBK

    fmadds      fr21,fr21, fr14ShiftR12,fr5 // fr21 = SUMZ >> 12 + BBK, 0
    ps_madd     fr19,fr19, fr14ShiftR12,fr3 // fr19 = SUMY >> 12 + GBK, SUMX >> 12 + RBK

    fmadds      fr24,fr24, fr14ShiftR12,fr5 // fr24 = SUMZ >> 12 + BBK, 0
    ps_madd     fr22,fr22, fr14ShiftR12,fr3 // fr22 = SUMY >> 12 + GBK, SUMX >> 12 + RBK

    // Save MACX, MACY, MACZ to tmpGteAddr
    psq_st      fr13, 8(r4), 1, 7    // save MACZ to tmp gteIR3(r4 + 4 * 2)
    psq_st      fr9, 12(r4), 0, 7    // save MACY, MACX to tmp gteIR2, gteIR1(r4 + 4 * 3)
    psq_st      fr21, 16(r4), 1, 7   // save MACZ to tmp gteIR3(r4 + 4 * 4)
    psq_st      fr19, 20(r4), 0, 7   // save MACY, MACX to tmp gteIR2, gteIR1(r4 + 4 * 5)
    psq_st      fr24, 24(r4), 1, 7   // save MACZ to tmp gteIR3(r4 + 4 * 6)
    psq_st      fr22, 28(r4), 0, 7   // save MACY, MACX to tmp gteIR2, gteIR1(r4 + 4 * 7)

    // RGB * IR (gteG (regs->CP2D.p[6].b.h))
    psq_l       fr0, 26(r3), 0, 2    // fr0 = gteG, gteR GQR2 = 0(u8)
    psq_l       fr3, 25(r3), 1, 2    // fr3 = gteB, 1
    psq_l       fr13, 8(r4), 1, 7    // load tmp gteIR3(r4 + 4 * 2)
    psq_l       fr9, 12(r4), 0, 7    // load tmp gteIR2, gteIR1(r4 + 4 * 3)
    psq_l       fr21, 16(r4), 1, 7   // load tmp gteIR3(r4 + 4 * 4)
    psq_l       fr19, 20(r4), 0, 7   // load tmp gteIR2, gteIR1(r4 + 4 * 5)
    psq_l       fr24, 24(r4), 1, 7   // load tmp gteIR3(r4 + 4 * 6)
    psq_l       fr22, 28(r4), 0, 7   // load tmp gteIR2, gteIR1(r4 + 4 * 7)

    fmuls       fr13, fr3, fr13      // fr13 = gteB * gteIR3
    ps_mul      fr9, fr0, fr9        // fr9 = gteG * gteIR2, gteR * gteIR1
    fmuls       fr21, fr3, fr21      // fr21 = gteB * gteIR3
    ps_mul      fr19, fr0, fr19      // fr19 = gteG * gteIR2, gteR * gteIR1
    fmuls       fr24, fr3, fr24      // fr24 = gteB * gteIR3
    ps_mul      fr22, fr0, fr22      // fr22 = gteG * gteIR2, gteR * gteIR1

    // gteIR1 = gteMAC1 = ((s32)gteR * gteIR1) >> 8
    // gteIR2 = gteMAC2 = ((s32)gteG * gteIR2) >> 8
    // gteIR3 = gteMAC3 = ((s32)gteB * gteIR3) >> 8
    ps_mul      fr0, fr9, fr17ShiftR8 // fr0 = gteMAC2 >> 8, gteMAC1 >> 8
    fmuls       fr1, fr13,fr17ShiftR8 // fr1 = gteMAC3 >> 8

    ps_mul      fr2, fr19,fr17ShiftR8 // fr2 = gteMAC2 >> 8, gteMAC1 >> 8
    fmuls       fr3, fr21,fr17ShiftR8 // fr3 = gteMAC3 >> 8

    ps_mul      fr4, fr22,fr17ShiftR8 // fr4 = gteMAC2 >> 8, gteMAC1 >> 8
    fmuls       fr5, fr24,fr17ShiftR8 // fr5 = gteMAC3 >> 8

    ps_merge10  fr6, fr0, fr0         // fr6 = gteMAC1 >> 8, gteMAC2 >> 8
    ps_merge10  fr7, fr2, fr2         // fr7 = gteMAC1 >> 8, gteMAC2 >> 8
    ps_merge10  fr8, fr4, fr4         // fr8 = gteMAC1 >> 8, gteMAC2 >> 8

    // only v2 save to gteIR1, gteIR2, gteIR3
    psq_st      fr5, 46(r3), 1, 3    // gteIR3 (regs->CP2D.p[11].sw.l)
    psq_st      fr4, 42(r3), 1, 3    // gteIR2 (regs->CP2D.p[10].sw.l)
    psq_st      fr8, 38(r3), 1, 3    // gteIR1 (regs->CP2D.p[9].sw.l)

    // gteR2 = limC1(((s32)gteR * gteIR1) >> 8 >> 4);
    // gteG2 = limC2(((s32)gteG * gteIR2) >> 8 >> 4);
    // gteB2 = limC3(((s32)gteB * gteIR3) >> 8 >> 4);
    // gteCODE2 = gteCODE; gteCODE2 (regs->CP2D.p[20, 21, 22].b.h3)
    lbz         r5, 24(r3)           // gteCODE (regs->CP2D.p[6].b.h3)
    psq_st      fr13, 81(r3), 1, 5   // gteB2  (regs->CP2D.p[20].b.h2)
    psq_st      fr9, 82(r3), 0, 5    // gteG2  (regs->CP2D.p[20].b.h), gteR2  (regs->CP2D.p[20].b.l)
    stb         r5, 80(r3)           // gteCODE2 (regs->CP2D.p[20].b.h3)

    psq_st      fr21, 85(r3), 1, 5   // gteB2  (regs->CP2D.p[21].b.h2)
    psq_st      fr19, 86(r3), 0, 5   // gteG2  (regs->CP2D.p[21].b.h), gteR2  (regs->CP2D.p[21].b.l)
    stb         r5, 84(r3)           // gteCODE2 (regs->CP2D.p[21].b.h3)

    psq_st      fr24, 89(r3), 1, 5   // gteB2  (regs->CP2D.p[22].b.h2)
    psq_st      fr22, 90(r3), 0, 5   // gteG2  (regs->CP2D.p[22].b.h), gteR2  (regs->CP2D.p[22].b.l)
    stb         r5, 88(r3)           // gteCODE2 (regs->CP2D.p[22].b.h3)

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
UnitZero:
	.float	0.0
UnitShiftL12:
	.float	4096.0                   // 1 << 12 = 4096.0
UnitShiftR5:
	.float	0.03125                  // 1 >> 5 = 1 / 32 = 0.03125
UnitShiftR8:
	.float	0.00390625               // 1 >> 8 = 1 / 256 = 0.00390625
UnitShiftR12:
	.float	0.000244140625           // 1 >> 12 = 1 / 4096 = 0.000244140625
UnitShiftR16:
	.float	0.00001526               // 1 >> 16 = 1 / 65536 = 0.00001526
DivOverflow:
	.int	0x1ffff

