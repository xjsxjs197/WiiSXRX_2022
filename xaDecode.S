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


.globl psDecodeBlock16
// r3: decodeTmp(IK0, IK1, vblockp, range, decp, x0, x1, x2, x3)
// r4: destp
// r5: inc
psDecodeBlock16:
    lwz         r9, 8(r3)           // vblockp
    lwz         r8, 16(r3)          // decp
    subi        r9, r9, 2
    sub         r4, r4, r5
    lhau        r10, 2(r9)          // load y

    // load fy0, fy1
    lfs         fr2, 0(r8)          // fr2 = fy0
    lfs         fr3, 4(r8)          // fr3 = fy1

    // load IK0, IK1
    lfs         fr0, 0(r3)          // fr0 = IK0
    lfs         fr1, 4(r3)          // fr1 = IK1

    // loop 1
    // check x0~x3
    lwz         r8, 12(r3)          // range
    rlwinm      r6, r10, 12, 16, 19 // (y << 12) & 0xf000
    rlwinm      r7, r10, 8, 16, 19  // (y << 8) & 0xf000
    extsh       r6, r6
    extsh       r7, r7
    srw         r6, r6, r8          // x0
    srw         r7, r7, r8          // x1
    sth         r6, 20(r3)          // save x0
    sth         r7, 22(r3)          // save x1

    rlwinm      r6, r10, 4, 16, 19  // (y << 4) & 0xf000
    rlwinm      r7, r10, 0, 16, 19  // (y << 0) & 0xf000
    extsh       r6, r6
    extsh       r7, r7
    srw         r6, r6, r8          // x2
    srw         r7, r7, r8          // x3
    sth         r6, 24(r3)          // save x2
    sth         r7, 26(r3)          // save x3

    // load x0~x3 (short => float)
    psq_l       fr6, 20(r3), 0, 3   // fr6 = x0, x1
    psq_l       fr7, 24(r3), 0, 3   // fr7 = x2, x3

    fmadds      fr4, fr1, fr3, fr6  // fr4 = x0 + IK1 * fy1
    ps_merge10  fr10, fr6, fr6      // fr10 = x1
    fmadds      fr8, fr0, fr2, fr4  // fr8 = x0 + IK0 * fy0 + IK1 * fy1   new x0
    ps_merge10  fr11, fr7, fr7      // fr11 = x3

    fmadds      fr9, fr1, fr2, fr10 // fr9 = x1 + IK1 * new fy1           for x1

    // save x0
    psq_stux    fr8, r4, r5, 1, 3   // destp + 0

    fmadds      fr9, fr0, fr8, fr9  // fr9 = x1 + IK0 * fy0 + IK1 * fy1   new x1
    fmadds      fr12, fr1, fr8, fr7 // fr12 = x2 + IK1 * new fy1          for x2

    // save x1
    psq_stux    fr9, r4, r5, 1, 3   // destp + inc

    fmadds      fr3, fr0, fr9, fr12 // fr3 = x2 + IK0 * fy0 + IK1 * fy1   new x2
    fmadds      fr12, fr1, fr9, fr11// fr12 = x3 + IK1 *  new fy1         for x3

    // save x2
    psq_stux    fr3, r4, r5, 1, 3   // destp + inc
    fmadds      fr2, fr0, fr3, fr12 // fr2 = x3 + IK0 * fy0 + IK1 * fy1   new x3

    lhau        r10, 2(r9)          // load y(next loop)

    // save x3
    psq_stux    fr2, r4, r5, 1, 3   // destp + inc

    // loop 2
    // check x0~x3
    rlwinm      r6, r10, 12, 16, 19 // (y << 12) & 0xf000
    rlwinm      r7, r10, 8, 16, 19  // (y << 8) & 0xf000
    extsh       r6, r6
    extsh       r7, r7
    srw         r6, r6, r8          // x0
    srw         r7, r7, r8          // x1
    sth         r6, 20(r3)          // save x0
    sth         r7, 22(r3)          // save x1

    rlwinm      r6, r10, 4, 16, 19  // (y << 4) & 0xf000
    rlwinm      r7, r10, 0, 16, 19  // (y << 0) & 0xf000
    extsh       r6, r6
    extsh       r7, r7
    srw         r6, r6, r8          // x2
    srw         r7, r7, r8          // x3
    sth         r6, 24(r3)          // save x2
    sth         r7, 26(r3)          // save x3

    // load x0~x3 (short => float)
    psq_l       fr6, 20(r3), 0, 3   // fr6 = x0, x1
    psq_l       fr7, 24(r3), 0, 3   // fr7 = x2, x3

    fmadds      fr4, fr1, fr3, fr6  // fr4 = x0 + IK1 * fy1
    ps_merge10  fr10, fr6, fr6      // fr10 = x1
    fmadds      fr8, fr0, fr2, fr4  // fr8 = x0 + IK0 * fy0 + IK1 * fy1   new x0
    ps_merge10  fr11, fr7, fr7      // fr11 = x3

    fmadds      fr9, fr1, fr2, fr10 // fr9 = x1 + IK1 * new fy1           for x1

    // save x0
    psq_stux    fr8, r4, r5, 1, 3   // destp + 0

    fmadds      fr9, fr0, fr8, fr9  // fr9 = x1 + IK0 * fy0 + IK1 * fy1   new x1
    fmadds      fr12, fr1, fr8, fr7 // fr12 = x2 + IK1 * new fy1          for x2

    // save x1
    psq_stux    fr9, r4, r5, 1, 3   // destp + inc

    fmadds      fr3, fr0, fr9, fr12 // fr3 = x2 + IK0 * fy0 + IK1 * fy1   new x2
    fmadds      fr12, fr1, fr9, fr11// fr12 = x3 + IK1 *  new fy1         for x3

    // save x2
    psq_stux    fr3, r4, r5, 1, 3   // destp + inc
    fmadds      fr2, fr0, fr3, fr12 // fr2 = x3 + IK0 * fy0 + IK1 * fy1   new x3

    lhau        r10, 2(r9)          // load y(next loop)

    // save x3
    psq_stux    fr2, r4, r5, 1, 3   // destp + inc

    // loop 3
    // check x0~x3
    rlwinm      r6, r10, 12, 16, 19 // (y << 12) & 0xf000
    rlwinm      r7, r10, 8, 16, 19  // (y << 8) & 0xf000
    extsh       r6, r6
    extsh       r7, r7
    srw         r6, r6, r8          // x0
    srw         r7, r7, r8          // x1
    sth         r6, 20(r3)          // save x0
    sth         r7, 22(r3)          // save x1

    rlwinm      r6, r10, 4, 16, 19  // (y << 4) & 0xf000
    rlwinm      r7, r10, 0, 16, 19  // (y << 0) & 0xf000
    extsh       r6, r6
    extsh       r7, r7
    srw         r6, r6, r8          // x2
    srw         r7, r7, r8          // x3
    sth         r6, 24(r3)          // save x2
    sth         r7, 26(r3)          // save x3

    // load x0~x3 (short => float)
    psq_l       fr6, 20(r3), 0, 3   // fr6 = x0, x1
    psq_l       fr7, 24(r3), 0, 3   // fr7 = x2, x3

    fmadds      fr4, fr1, fr3, fr6  // fr4 = x0 + IK1 * fy1
    ps_merge10  fr10, fr6, fr6      // fr10 = x1
    fmadds      fr8, fr0, fr2, fr4  // fr8 = x0 + IK0 * fy0 + IK1 * fy1   new x0
    ps_merge10  fr11, fr7, fr7      // fr11 = x3

    fmadds      fr9, fr1, fr2, fr10 // fr9 = x1 + IK1 * new fy1           for x1

    // save x0
    psq_stux    fr8, r4, r5, 1, 3   // destp + 0

    fmadds      fr9, fr0, fr8, fr9  // fr9 = x1 + IK0 * fy0 + IK1 * fy1   new x1
    fmadds      fr12, fr1, fr8, fr7 // fr12 = x2 + IK1 * new fy1          for x2

    // save x1
    psq_stux    fr9, r4, r5, 1, 3   // destp + inc

    fmadds      fr3, fr0, fr9, fr12 // fr3 = x2 + IK0 * fy0 + IK1 * fy1   new x2
    fmadds      fr12, fr1, fr9, fr11// fr12 = x3 + IK1 *  new fy1         for x3

    // save x2
    psq_stux    fr3, r4, r5, 1, 3   // destp + inc
    fmadds      fr2, fr0, fr3, fr12 // fr2 = x3 + IK0 * fy0 + IK1 * fy1   new x3

    lhau        r10, 2(r9)          // load y(next loop)

    // save x3
    psq_stux    fr2, r4, r5, 1, 3   // destp + inc

    // loop 4
    // check x0~x3
    rlwinm      r6, r10, 12, 16, 19 // (y << 12) & 0xf000
    rlwinm      r7, r10, 8, 16, 19  // (y << 8) & 0xf000
    extsh       r6, r6
    extsh       r7, r7
    srw         r6, r6, r8          // x0
    srw         r7, r7, r8          // x1
    sth         r6, 20(r3)          // save x0
    sth         r7, 22(r3)          // save x1

    rlwinm      r6, r10, 4, 16, 19  // (y << 4) & 0xf000
    rlwinm      r7, r10, 0, 16, 19  // (y << 0) & 0xf000
    extsh       r6, r6
    extsh       r7, r7
    srw         r6, r6, r8          // x2
    srw         r7, r7, r8          // x3
    sth         r6, 24(r3)          // save x2
    sth         r7, 26(r3)          // save x3

    // load x0~x3 (short => float)
    psq_l       fr6, 20(r3), 0, 3   // fr6 = x0, x1
    psq_l       fr7, 24(r3), 0, 3   // fr7 = x2, x3

    fmadds      fr4, fr1, fr3, fr6  // fr4 = x0 + IK1 * fy1
    ps_merge10  fr10, fr6, fr6      // fr10 = x1
    fmadds      fr8, fr0, fr2, fr4  // fr8 = x0 + IK0 * fy0 + IK1 * fy1   new x0
    ps_merge10  fr11, fr7, fr7      // fr11 = x3

    fmadds      fr9, fr1, fr2, fr10 // fr9 = x1 + IK1 * new fy1           for x1

    // save x0
    psq_stux    fr8, r4, r5, 1, 3   // destp + 0

    fmadds      fr9, fr0, fr8, fr9  // fr9 = x1 + IK0 * fy0 + IK1 * fy1   new x1
    fmadds      fr12, fr1, fr8, fr7 // fr12 = x2 + IK1 * new fy1          for x2

    // save x1
    psq_stux    fr9, r4, r5, 1, 3   // destp + inc

    fmadds      fr3, fr0, fr9, fr12 // fr3 = x2 + IK0 * fy0 + IK1 * fy1   new x2
    fmadds      fr12, fr1, fr9, fr11// fr12 = x3 + IK1 *  new fy1         for x3

    // save x2
    psq_stux    fr3, r4, r5, 1, 3   // destp + inc
    fmadds      fr2, fr0, fr3, fr12 // fr2 = x3 + IK0 * fy0 + IK1 * fy1   new x3

    lhau        r10, 2(r9)          // load y(next loop)

    // save x3
    psq_stux    fr2, r4, r5, 1, 3   // destp + inc

    // loop 5
    // check x0~x3
    rlwinm      r6, r10, 12, 16, 19 // (y << 12) & 0xf000
    rlwinm      r7, r10, 8, 16, 19  // (y << 8) & 0xf000
    extsh       r6, r6
    extsh       r7, r7
    srw         r6, r6, r8          // x0
    srw         r7, r7, r8          // x1
    sth         r6, 20(r3)          // save x0
    sth         r7, 22(r3)          // save x1

    rlwinm      r6, r10, 4, 16, 19  // (y << 4) & 0xf000
    rlwinm      r7, r10, 0, 16, 19  // (y << 0) & 0xf000
    extsh       r6, r6
    extsh       r7, r7
    srw         r6, r6, r8          // x2
    srw         r7, r7, r8          // x3
    sth         r6, 24(r3)          // save x2
    sth         r7, 26(r3)          // save x3

    // load x0~x3 (short => float)
    psq_l       fr6, 20(r3), 0, 3   // fr6 = x0, x1
    psq_l       fr7, 24(r3), 0, 3   // fr7 = x2, x3

    fmadds      fr4, fr1, fr3, fr6  // fr4 = x0 + IK1 * fy1
    ps_merge10  fr10, fr6, fr6      // fr10 = x1
    fmadds      fr8, fr0, fr2, fr4  // fr8 = x0 + IK0 * fy0 + IK1 * fy1   new x0
    ps_merge10  fr11, fr7, fr7      // fr11 = x3

    fmadds      fr9, fr1, fr2, fr10 // fr9 = x1 + IK1 * new fy1           for x1

    // save x0
    psq_stux    fr8, r4, r5, 1, 3   // destp + 0

    fmadds      fr9, fr0, fr8, fr9  // fr9 = x1 + IK0 * fy0 + IK1 * fy1   new x1
    fmadds      fr12, fr1, fr8, fr7 // fr12 = x2 + IK1 * new fy1          for x2

    // save x1
    psq_stux    fr9, r4, r5, 1, 3   // destp + inc

    fmadds      fr3, fr0, fr9, fr12 // fr3 = x2 + IK0 * fy0 + IK1 * fy1   new x2
    fmadds      fr12, fr1, fr9, fr11// fr12 = x3 + IK1 *  new fy1         for x3

    // save x2
    psq_stux    fr3, r4, r5, 1, 3   // destp + inc
    fmadds      fr2, fr0, fr3, fr12 // fr2 = x3 + IK0 * fy0 + IK1 * fy1   new x3

    lhau        r10, 2(r9)          // load y(next loop)

    // save x3
    psq_stux    fr2, r4, r5, 1, 3   // destp + inc

    // loop 6
    // check x0~x3
    rlwinm      r6, r10, 12, 16, 19 // (y << 12) & 0xf000
    rlwinm      r7, r10, 8, 16, 19  // (y << 8) & 0xf000
    extsh       r6, r6
    extsh       r7, r7
    srw         r6, r6, r8          // x0
    srw         r7, r7, r8          // x1
    sth         r6, 20(r3)          // save x0
    sth         r7, 22(r3)          // save x1

    rlwinm      r6, r10, 4, 16, 19  // (y << 4) & 0xf000
    rlwinm      r7, r10, 0, 16, 19  // (y << 0) & 0xf000
    extsh       r6, r6
    extsh       r7, r7
    srw         r6, r6, r8          // x2
    srw         r7, r7, r8          // x3
    sth         r6, 24(r3)          // save x2
    sth         r7, 26(r3)          // save x3

    // load x0~x3 (short => float)
    psq_l       fr6, 20(r3), 0, 3   // fr6 = x0, x1
    psq_l       fr7, 24(r3), 0, 3   // fr7 = x2, x3

    fmadds      fr4, fr1, fr3, fr6  // fr4 = x0 + IK1 * fy1
    ps_merge10  fr10, fr6, fr6      // fr10 = x1
    fmadds      fr8, fr0, fr2, fr4  // fr8 = x0 + IK0 * fy0 + IK1 * fy1   new x0
    ps_merge10  fr11, fr7, fr7      // fr11 = x3

    fmadds      fr9, fr1, fr2, fr10 // fr9 = x1 + IK1 * new fy1           for x1

    // save x0
    psq_stux    fr8, r4, r5, 1, 3   // destp + 0

    fmadds      fr9, fr0, fr8, fr9  // fr9 = x1 + IK0 * fy0 + IK1 * fy1   new x1
    fmadds      fr12, fr1, fr8, fr7 // fr12 = x2 + IK1 * new fy1          for x2

    // save x1
    psq_stux    fr9, r4, r5, 1, 3   // destp + inc

    fmadds      fr3, fr0, fr9, fr12 // fr3 = x2 + IK0 * fy0 + IK1 * fy1   new x2
    fmadds      fr12, fr1, fr9, fr11// fr12 = x3 + IK1 *  new fy1         for x3

    // save x2
    psq_stux    fr3, r4, r5, 1, 3   // destp + inc
    fmadds      fr2, fr0, fr3, fr12 // fr2 = x3 + IK0 * fy0 + IK1 * fy1   new x3

    lhau        r10, 2(r9)          // load y(next loop)

    // save x3
    psq_stux    fr2, r4, r5, 1, 3   // destp + inc

    // loop 7
    // check x0~x3
    rlwinm      r6, r10, 12, 16, 19 // (y << 12) & 0xf000
    rlwinm      r7, r10, 8, 16, 19  // (y << 8) & 0xf000
    extsh       r6, r6
    extsh       r7, r7
    srw         r6, r6, r8          // x0
    srw         r7, r7, r8          // x1
    sth         r6, 20(r3)          // save x0
    sth         r7, 22(r3)          // save x1

    rlwinm      r6, r10, 4, 16, 19  // (y << 4) & 0xf000
    rlwinm      r7, r10, 0, 16, 19  // (y << 0) & 0xf000
    extsh       r6, r6
    extsh       r7, r7
    srw         r6, r6, r8          // x2
    srw         r7, r7, r8          // x3
    sth         r6, 24(r3)          // save x2
    sth         r7, 26(r3)          // save x3

    // load x0~x3 (short => float)
    psq_l       fr6, 20(r3), 0, 3   // fr6 = x0, x1
    psq_l       fr7, 24(r3), 0, 3   // fr7 = x2, x3

    fmadds      fr4, fr1, fr3, fr6  // fr4 = x0 + IK1 * fy1
    ps_merge10  fr10, fr6, fr6      // fr10 = x1
    fmadds      fr8, fr0, fr2, fr4  // fr8 = x0 + IK0 * fy0 + IK1 * fy1   new x0
    ps_merge10  fr11, fr7, fr7      // fr11 = x3

    fmadds      fr9, fr1, fr2, fr10 // fr9 = x1 + IK1 * new fy1           for x1

    // save x0
    psq_stux    fr8, r4, r5, 1, 3   // destp + 0

    fmadds      fr9, fr0, fr8, fr9  // fr9 = x1 + IK0 * fy0 + IK1 * fy1   new x1
    fmadds      fr12, fr1, fr8, fr7 // fr12 = x2 + IK1 * new fy1          for x2

    // save x1
    psq_stux    fr9, r4, r5, 1, 3   // destp + inc

    fmadds      fr3, fr0, fr9, fr12 // fr3 = x2 + IK0 * fy0 + IK1 * fy1   new x2
    fmadds      fr12, fr1, fr9, fr11// fr12 = x3 + IK1 *  new fy1         for x3

    // save x2
    psq_stux    fr3, r4, r5, 1, 3   // destp + inc
    fmadds      fr2, fr0, fr3, fr12 // fr2 = x3 + IK0 * fy0 + IK1 * fy1   new x3

    lwz         r8, 16(r3)          // decp

    // save x3
    psq_stux    fr2, r4, r5, 1, 3   // destp + inc

    // save fy1
    stfs        fr3, 4(r8)
    // save fy0
    stfs        fr2, 0(r8)

    blr

